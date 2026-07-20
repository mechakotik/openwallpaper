use std::{ffi::c_void, io::ErrorKind, ptr::NonNull, sync::Arc};

use anyhow::{Context, Result, bail};
use raw_window_handle::{
    DisplayHandle, HandleError, HasDisplayHandle, HasWindowHandle, RawDisplayHandle,
    RawWindowHandle, WaylandDisplayHandle, WaylandWindowHandle, WindowHandle,
};
use rustix::event::{PollFd, PollFlags, Timespec, poll};
use wayland_client::{
    Connection, Dispatch, EventQueue, Proxy, QueueHandle, WEnum,
    backend::WaylandError,
    delegate_noop,
    globals::{Global, GlobalListContents, registry_queue_init},
    protocol::{wl_compositor, wl_output, wl_registry, wl_surface},
};
use wayland_protocols_wlr::layer_shell::v1::client::{zwlr_layer_shell_v1, zwlr_layer_surface_v1};

use crate::output::{NativeWindow, Output, OutputState};

const DEFAULT_NAMESPACE: &str = "wallpaperd";
const WL_OUTPUT_VERSION: u32 = 4;

fn oriented_pixel_size(mode_size: (u32, u32), transform: wl_output::Transform) -> (u32, u32) {
    match transform {
        wl_output::Transform::_90
        | wl_output::Transform::_270
        | wl_output::Transform::Flipped90
        | wl_output::Transform::Flipped270 => (mode_size.1, mode_size.0),
        _ => mode_size,
    }
}

pub(crate) struct WlrOutput {
    connection: Connection,
    event_queue: EventQueue<WlrState>,
    state: WlrState,
}

impl WlrOutput {
    pub(crate) fn list_displays() -> Result<Vec<String>> {
        let connection =
            Connection::connect_to_env().context("connecting to the Wayland compositor")?;
        let (globals, mut event_queue) = registry_queue_init::<DisplayListState>(&connection)
            .context("reading the Wayland registry")?;
        let queue_handle = event_queue.handle();
        let mut state = DisplayListState::default();

        for global in globals.contents().clone_list() {
            if global.interface != wl_output::WlOutput::interface().name {
                continue;
            }
            if global.version < WL_OUTPUT_VERSION {
                bail!("wl_output v4 is required to enumerate displays by name");
            }

            globals.registry().bind::<wl_output::WlOutput, _, _>(
                global.name,
                WL_OUTPUT_VERSION,
                &queue_handle,
                (),
            );
        }

        event_queue
            .roundtrip(&mut state)
            .context("reading Wayland output names")?;
        state.names.sort();
        state.names.dedup();
        Ok(state.names)
    }

    /// If the output is absent, the initial state has no window. The object
    /// creates one when that output later appears. An empty name selects the
    /// first available output.
    pub(crate) fn new(output_name: impl Into<String>) -> Result<Self> {
        let output_name = output_name.into();
        let connection =
            Connection::connect_to_env().context("connecting to the Wayland compositor")?;
        let (globals, mut event_queue) =
            registry_queue_init::<WlrState>(&connection).context("reading the Wayland registry")?;
        let queue_handle = event_queue.handle();

        let compositor = globals
            .bind::<wl_compositor::WlCompositor, _, _>(&queue_handle, 1..=6, ())
            .context("wl_compositor is unavailable")?;
        let layer_shell = globals
            .bind::<zwlr_layer_shell_v1::ZwlrLayerShellV1, _, _>(&queue_handle, 1..=5, ())
            .context("zwlr_layer_shell_v1 is unavailable")?;

        let mut state = WlrState::new(output_name, compositor, layer_shell);
        let advertised_globals = globals.contents().clone_list();
        if advertised_globals.iter().any(|global| {
            global.interface == wl_output::WlOutput::interface().name
                && global.version < WL_OUTPUT_VERSION
        }) {
            bail!("wl_output v4 is required to select displays by name");
        }
        for global in advertised_globals {
            state.bind_output(globals.registry(), &global, &queue_handle);
        }

        event_queue
            .roundtrip(&mut state)
            .context("reading Wayland output names")?;
        state.reconcile(&connection, &queue_handle);
        Self::flush(&event_queue).context("creating the Wayland wallpaper surface")?;

        Ok(Self {
            connection,
            event_queue,
            state,
        })
    }

    fn pump_events(&mut self) -> Result<()> {
        let connection = &self.connection;
        let event_queue = &mut self.event_queue;
        let queue_handle = event_queue.handle();

        event_queue
            .dispatch_pending(&mut self.state)
            .context("dispatching pending Wayland events")?;
        self.state.reconcile(connection, &queue_handle);
        Self::flush(event_queue)?;

        let Some(read_guard) = event_queue.prepare_read() else {
            event_queue
                .dispatch_pending(&mut self.state)
                .context("dispatching Wayland events before reading")?;
            self.state.reconcile(connection, &queue_handle);
            Self::flush(event_queue)?;
            return Ok(());
        };

        let ready = {
            let socket = read_guard.connection_fd();
            let mut descriptors = [PollFd::new(
                &socket,
                PollFlags::IN | PollFlags::ERR | PollFlags::HUP,
            )];
            let timeout = Timespec::default();

            match poll(&mut descriptors, Some(&timeout)) {
                Ok(0) => false,
                Ok(_) => descriptors[0]
                    .revents()
                    .intersects(PollFlags::IN | PollFlags::ERR | PollFlags::HUP),
                Err(rustix::io::Errno::INTR) => false,
                Err(error) => return Err(error).context("polling the Wayland socket"),
            }
        };

        if !ready {
            drop(read_guard);
            return Ok(());
        }

        match read_guard.read() {
            Ok(_) => {}
            Err(WaylandError::Io(error)) if error.kind() == ErrorKind::WouldBlock => return Ok(()),
            Err(error) => return Err(error).context("reading Wayland events"),
        }

        event_queue
            .dispatch_pending(&mut self.state)
            .context("dispatching Wayland events after reading")?;
        self.state.reconcile(connection, &queue_handle);
        Self::flush(event_queue)?;
        Ok(())
    }

    fn flush(event_queue: &EventQueue<WlrState>) -> Result<()> {
        match event_queue.flush() {
            Ok(()) => Ok(()),
            Err(WaylandError::Io(error)) if error.kind() == ErrorKind::WouldBlock => Ok(()),
            Err(error) => Err(error).context("flushing Wayland requests"),
        }
    }
}

impl Output for WlrOutput {
    fn update(&mut self) -> Result<()> {
        self.pump_events()
    }

    fn state(&self) -> &OutputState {
        &self.state.output
    }
}

impl Drop for WlrOutput {
    fn drop(&mut self) {
        self.state.shutdown_connection();
        let _ = self.event_queue.flush();
    }
}

struct WlrState {
    output_name: String,
    output: OutputState,
    compositor: wl_compositor::WlCompositor,
    layer_shell: zwlr_layer_shell_v1::ZwlrLayerShellV1,
    outputs: Vec<KnownOutput>,
    surface: Option<LayerSurface>,
}

struct KnownOutput {
    global_name: u32,
    proxy: wl_output::WlOutput,
    name: Option<String>,
    mode_size: Option<(u32, u32)>,
    pending_mode_size: Option<(u32, u32)>,
    transform: wl_output::Transform,
    pending_transform: Option<wl_output::Transform>,
    ready: bool,
    closed: bool,
}

impl KnownOutput {
    fn pixel_size(&self) -> Option<(u32, u32)> {
        self.mode_size
            .map(|mode_size| oriented_pixel_size(mode_size, self.transform))
    }
}

struct LayerSurface {
    output_global_name: u32,
    window: Arc<WlrWindow>,
    role: zwlr_layer_surface_v1::ZwlrLayerSurfaceV1,
    configured: bool,
}

impl WlrState {
    fn new(
        output_name: String,
        compositor: wl_compositor::WlCompositor,
        layer_shell: zwlr_layer_shell_v1::ZwlrLayerShellV1,
    ) -> Self {
        Self {
            output_name,
            output: OutputState::default(),
            compositor,
            layer_shell,
            outputs: Vec::new(),
            surface: None,
        }
    }

    fn bind_output(
        &mut self,
        registry: &wl_registry::WlRegistry,
        global: &Global,
        queue_handle: &QueueHandle<Self>,
    ) {
        if global.interface != wl_output::WlOutput::interface().name
            || global.version < WL_OUTPUT_VERSION
        {
            return;
        }
        if self
            .outputs
            .iter()
            .any(|output| output.global_name == global.name)
        {
            return;
        }

        let proxy = registry.bind::<wl_output::WlOutput, _, _>(
            global.name,
            WL_OUTPUT_VERSION,
            queue_handle,
            global.name,
        );
        self.outputs.push(KnownOutput {
            global_name: global.name,
            proxy,
            name: None,
            mode_size: None,
            pending_mode_size: None,
            transform: wl_output::Transform::Normal,
            pending_transform: None,
            ready: false,
            closed: false,
        });
    }

    fn desired_output(&self) -> Option<&KnownOutput> {
        self.outputs.iter().find(|output| {
            output.ready
                && !output.closed
                && (self.output_name.is_empty()
                    || output.name.as_deref() == Some(self.output_name.as_str()))
        })
    }

    fn reconcile(&mut self, connection: &Connection, queue_handle: &QueueHandle<Self>) {
        let desired_output = self.desired_output().map(|output| output.global_name);
        let current_output = self
            .surface
            .as_ref()
            .map(|surface| surface.output_global_name);

        if current_output != desired_output {
            self.destroy_surface();
        }

        if self.surface.is_none()
            && let Some(output_global_name) = desired_output
        {
            self.create_surface(connection.clone(), queue_handle, output_global_name);
        }

        self.apply_surface_state();
    }

    fn create_surface(
        &mut self,
        connection: Connection,
        queue_handle: &QueueHandle<Self>,
        output_global_name: u32,
    ) {
        let output_proxy = self
            .outputs
            .iter()
            .find(|output| output.global_name == output_global_name)
            .map(|output| output.proxy.clone())
            .expect("create_surface called without the selected output");

        let wl_surface = self.compositor.create_surface(queue_handle, ());

        let window = Arc::new(WlrWindow {
            connection,
            surface: wl_surface,
        });
        let role = self.layer_shell.get_layer_surface(
            &window.surface,
            Some(&output_proxy),
            zwlr_layer_shell_v1::Layer::Background,
            DEFAULT_NAMESPACE.to_owned(),
            queue_handle,
            (),
        );
        role.set_size(0, 0);
        role.set_anchor(
            zwlr_layer_surface_v1::Anchor::Top
                | zwlr_layer_surface_v1::Anchor::Bottom
                | zwlr_layer_surface_v1::Anchor::Left
                | zwlr_layer_surface_v1::Anchor::Right,
        );
        role.set_exclusive_zone(-1);

        // Layer-shell requires a bufferless initial commit. The renderer may
        // create/use its GPU surface only after the configure event.
        window.surface.commit();

        self.surface = Some(LayerSurface {
            output_global_name,
            window,
            role,
            configured: false,
        });
    }

    fn output_named(&mut self, global_name: u32, name: String) {
        let Some(output) = self
            .outputs
            .iter_mut()
            .find(|output| output.global_name == global_name)
        else {
            return;
        };
        output.name = Some(name);
    }

    fn output_mode(
        &mut self,
        global_name: u32,
        flags: WEnum<wl_output::Mode>,
        width: i32,
        height: i32,
    ) {
        let flags = match flags {
            WEnum::Value(flags) => flags.bits(),
            WEnum::Unknown(flags) => flags,
        };
        if flags & wl_output::Mode::Current.bits() == 0 {
            return;
        }
        let (Ok(width), Ok(height)) = (u32::try_from(width), u32::try_from(height)) else {
            return;
        };
        if width == 0 || height == 0 {
            return;
        }

        if let Some(output) = self
            .outputs
            .iter_mut()
            .find(|output| output.global_name == global_name)
        {
            output.pending_mode_size = Some((width, height));
        }
    }

    fn output_geometry(&mut self, global_name: u32, transform: WEnum<wl_output::Transform>) {
        let WEnum::Value(transform) = transform else {
            return;
        };
        if let Some(output) = self
            .outputs
            .iter_mut()
            .find(|output| output.global_name == global_name)
        {
            output.pending_transform = Some(transform);
        }
    }

    fn output_done(&mut self, global_name: u32) {
        let Some(output) = self
            .outputs
            .iter_mut()
            .find(|output| output.global_name == global_name)
        else {
            return;
        };
        if let Some(mode_size) = output.pending_mode_size.take() {
            output.mode_size = Some(mode_size);
        }
        if let Some(transform) = output.pending_transform.take() {
            output.transform = transform;
        }
        output.ready = true;
    }

    fn output_removed(&mut self, global_name: u32) {
        let Some(index) = self
            .outputs
            .iter()
            .position(|output| output.global_name == global_name)
        else {
            return;
        };

        let output = self.outputs.swap_remove(index);
        output.proxy.release();
    }

    fn destroy_surface(&mut self) {
        if let Some(surface) = self.surface.take() {
            surface.role.destroy();
        }
        self.output.withdraw();
    }

    fn configure_surface(&mut self) {
        if let Some(surface) = self.surface.as_mut() {
            surface.configured = true;
        }
    }

    fn close_surface(&mut self) {
        let Some(global_name) = self
            .surface
            .as_ref()
            .map(|surface| surface.output_global_name)
        else {
            return;
        };
        if let Some(output) = self
            .outputs
            .iter_mut()
            .find(|output| output.global_name == global_name)
        {
            output.closed = true;
        }
    }

    fn apply_surface_state(&mut self) {
        let Some(surface) = self.surface.as_ref() else {
            return;
        };
        if !surface.configured {
            return;
        }
        let Some((width, height)) = self
            .outputs
            .iter()
            .find(|output| output.global_name == surface.output_global_name)
            .and_then(KnownOutput::pixel_size)
        else {
            return;
        };

        if self.output.window.is_none() {
            let window: Arc<dyn NativeWindow> = surface.window.clone();
            self.output.publish(window, width, height);
        } else {
            self.output.resize(width, height);
        }
    }

    fn shutdown_connection(&mut self) {
        self.destroy_surface();
        for output in self.outputs.drain(..) {
            output.proxy.release();
        }
        if self.layer_shell.version() >= 3 {
            self.layer_shell.destroy();
        }
    }
}

impl Dispatch<wl_registry::WlRegistry, GlobalListContents> for WlrState {
    fn event(
        state: &mut Self,
        registry: &wl_registry::WlRegistry,
        event: wl_registry::Event,
        _data: &GlobalListContents,
        _connection: &Connection,
        queue_handle: &QueueHandle<Self>,
    ) {
        match event {
            wl_registry::Event::Global {
                name,
                interface,
                version,
            } => state.bind_output(
                registry,
                &Global {
                    name,
                    interface,
                    version,
                },
                queue_handle,
            ),
            wl_registry::Event::GlobalRemove { name } => state.output_removed(name),
            _ => {}
        }
    }
}

delegate_noop!(WlrState: ignore wl_compositor::WlCompositor);
delegate_noop!(WlrState: ignore zwlr_layer_shell_v1::ZwlrLayerShellV1);

impl Dispatch<wl_output::WlOutput, u32> for WlrState {
    fn event(
        state: &mut Self,
        _output: &wl_output::WlOutput,
        event: wl_output::Event,
        global_name: &u32,
        _connection: &Connection,
        _queue_handle: &QueueHandle<Self>,
    ) {
        match event {
            wl_output::Event::Geometry { transform, .. } => {
                state.output_geometry(*global_name, transform);
            }
            wl_output::Event::Mode {
                flags,
                width,
                height,
                ..
            } => state.output_mode(*global_name, flags, width, height),
            wl_output::Event::Name { name } => state.output_named(*global_name, name),
            wl_output::Event::Done => state.output_done(*global_name),
            _ => {}
        }
    }
}

impl Dispatch<wl_surface::WlSurface, ()> for WlrState {
    fn event(
        _state: &mut Self,
        _surface: &wl_surface::WlSurface,
        _event: wl_surface::Event,
        _data: &(),
        _connection: &Connection,
        _queue_handle: &QueueHandle<Self>,
    ) {
    }
}

impl Dispatch<zwlr_layer_surface_v1::ZwlrLayerSurfaceV1, ()> for WlrState {
    fn event(
        state: &mut Self,
        layer_surface: &zwlr_layer_surface_v1::ZwlrLayerSurfaceV1,
        event: zwlr_layer_surface_v1::Event,
        _data: &(),
        _connection: &Connection,
        _queue_handle: &QueueHandle<Self>,
    ) {
        if !state
            .surface
            .as_ref()
            .is_some_and(|surface| &surface.role == layer_surface)
        {
            return;
        }

        match event {
            zwlr_layer_surface_v1::Event::Configure { serial, .. } => {
                layer_surface.ack_configure(serial);
                state.configure_surface();
            }
            zwlr_layer_surface_v1::Event::Closed => {
                state.close_surface();
            }
            _ => {}
        }
    }
}

/// Owns everything whose lifetime is promised by its raw handles.
struct WlrWindow {
    connection: Connection,
    surface: wl_surface::WlSurface,
}

impl Drop for WlrWindow {
    fn drop(&mut self) {
        self.surface.destroy();
    }
}

impl HasDisplayHandle for WlrWindow {
    fn display_handle(&self) -> std::result::Result<DisplayHandle<'_>, HandleError> {
        let display = self.connection.backend().display_ptr().cast::<c_void>();
        let display = NonNull::new(display).ok_or(HandleError::Unavailable)?;
        let raw = RawDisplayHandle::Wayland(WaylandDisplayHandle::new(display));

        // SAFETY: WlrWindow owns a Connection clone, so the wl_display stays
        // valid for at least the lifetime of the returned borrowed handle.
        Ok(unsafe { DisplayHandle::borrow_raw(raw) })
    }
}

impl HasWindowHandle for WlrWindow {
    fn window_handle(&self) -> std::result::Result<WindowHandle<'_>, HandleError> {
        let surface = self.surface.id().as_ptr().cast::<c_void>();
        let surface = NonNull::new(surface).ok_or(HandleError::Unavailable)?;
        let raw = RawWindowHandle::Wayland(WaylandWindowHandle::new(surface));

        // SAFETY: WlrWindow owns the wl_surface proxy for at least the
        // lifetime of the returned borrowed handle.
        Ok(unsafe { WindowHandle::borrow_raw(raw) })
    }
}

#[derive(Default)]
struct DisplayListState {
    names: Vec<String>,
}

impl Dispatch<wl_registry::WlRegistry, GlobalListContents> for DisplayListState {
    fn event(
        _state: &mut Self,
        _registry: &wl_registry::WlRegistry,
        _event: wl_registry::Event,
        _data: &GlobalListContents,
        _connection: &Connection,
        _queue_handle: &QueueHandle<Self>,
    ) {
    }
}

impl Dispatch<wl_output::WlOutput, ()> for DisplayListState {
    fn event(
        state: &mut Self,
        _output: &wl_output::WlOutput,
        event: wl_output::Event,
        _data: &(),
        _connection: &Connection,
        _queue_handle: &QueueHandle<Self>,
    ) {
        if let wl_output::Event::Name { name } = event {
            state.names.push(name);
        }
    }
}
