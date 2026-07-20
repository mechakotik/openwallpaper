use std::sync::Arc;

use anyhow::Result;
use raw_window_handle::{HasDisplayHandle, HasWindowHandle};

pub(crate) trait NativeWindow: HasDisplayHandle + HasWindowHandle {}

impl<T> NativeWindow for T where T: HasDisplayHandle + HasWindowHandle {}

pub(crate) trait Output {
    fn update(&mut self) -> Result<()>;
    fn state(&self) -> &OutputState;
}

#[derive(Clone, Default)]
pub(crate) struct OutputState {
    pub(crate) generation: u64,
    pub(crate) window: Option<Arc<dyn NativeWindow>>,
    pub(crate) width: u32,
    pub(crate) height: u32,
    pub(crate) visible: bool,
}

impl OutputState {
    pub(crate) fn publish(&mut self, window: Arc<dyn NativeWindow>, width: u32, height: u32) {
        debug_assert!(self.window.is_none());
        self.window = Some(window);
        self.width = width;
        self.height = height;
        self.visible = true;
        self.advance_generation();
    }

    pub(crate) fn resize(&mut self, width: u32, height: u32) {
        self.width = width;
        self.height = height;
    }

    pub(crate) fn withdraw(&mut self) {
        if self.window.take().is_some() {
            self.advance_generation();
        }
        self.width = 0;
        self.height = 0;
        self.visible = false;
    }

    fn advance_generation(&mut self) {
        self.generation = self.generation.wrapping_add(1);
    }
}
