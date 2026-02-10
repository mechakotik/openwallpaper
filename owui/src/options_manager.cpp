#include "options_manager.h"
#include <QSettings>

OptionsManager::OptionsManager(QObject* parent) : QObject(parent) {
    connect(this, &OptionsManager::wallpaperdPathChanged, this, &OptionsManager::onWallpaperdPathChanged);
    connect(this, &OptionsManager::vSyncChanged, this, &OptionsManager::onVSyncChanged);
    connect(this, &OptionsManager::fpsLimitChanged, this, &OptionsManager::onFPSLimitChanged);
    connect(this, &OptionsManager::preferDiscreteGPUChanged, this, &OptionsManager::onPreferDiscreteGPUChanged);
    connect(this, &OptionsManager::pauseHiddenChanged, this, &OptionsManager::onPauseHiddenChanged);
    connect(this, &OptionsManager::pauseOnBatChanged, this, &OptionsManager::onPauseOnBatChanged);
    connect(this, &OptionsManager::audioVisualizationChanged, this, &OptionsManager::onAudioVisualizationChanged);
    connect(this, &OptionsManager::audioBackendChanged, this, &OptionsManager::onAudioBackendChanged);
    connect(this, &OptionsManager::audioSourceChanged, this, &OptionsManager::onAudioSourceChanged);
    connect(this, &OptionsManager::wpeCompilePathChanged, this, &OptionsManager::onWpeCompilePathChanged);
    connect(this, &OptionsManager::wasmCCPathChanged, this, &OptionsManager::onWasmCCPathChanged);
    connect(this, &OptionsManager::glslcPathChanged, this, &OptionsManager::onGlslcPathChanged);
    connect(this, &OptionsManager::wpeAssetsPathChanged, this, &OptionsManager::onWPEAssetsPathChanged);
    connect(this, &OptionsManager::wpeAutoImportChanged, this, &OptionsManager::onWPEAutoImportChanged);
    connect(this, &OptionsManager::wpeLibraryPathChanged, this, &OptionsManager::onWPELibraryPathChanged);

    auto ensure = [&](const char* key, const QVariant& def) {
        if(!settings.contains(key)) {
            settings.setValue(key, def);
        }
    };

    ensure("wallpaperdPath", "");
    ensure("vSync", false);
    ensure("fpsLimit", 30);
    ensure("preferDiscreteGPU", false);
    ensure("pauseHidden", true);
    ensure("pauseOnBat", false);
    ensure("audioVisualization", true);
    ensure("audioBackend", 0);
    ensure("audioSource", "");
    ensure("wpeCompilePath", "");
    ensure("wasmCCPath", "");
    ensure("glslcPath", "");
    ensure("wpeAssetsPath", "");
    ensure("wpeAutoImport", true);
    ensure("wpeLibraryPath", "");

    mWallpaperdPath = settings.value("wallpaperdPath").toString();
    mVSync = settings.value("vSync").toBool();
    mFPSLimit = settings.value("fpsLimit").toInt();
    mPreferDiscreteGPU = settings.value("preferDiscreteGPU").toBool();
    mPauseHidden = settings.value("pauseHidden").toBool();
    mPauseOnBat = settings.value("pauseOnBat").toBool();
    mAudioVisualization = settings.value("audioVisualization").toBool();
    mAudioBackend = settings.value("audioBackend").toInt();
    mAudioSource = settings.value("audioSource").toString();
    mWpeCompilePath = settings.value("wpeCompilePath").toString();
    mWasmCCPath = settings.value("wasmCCPath").toString();
    mGlslcPath = settings.value("glslcPath").toString();
    mWPEAssetsPath = settings.value("wpeAssetsPath").toString();
    mWPEAutoImport = settings.value("wpeAutoImport").toBool();
    mWPELibraryPath = settings.value("wpeLibraryPath").toString();

    Q_EMIT wallpaperdPathChanged();
    Q_EMIT vSyncChanged();
    Q_EMIT fpsLimitChanged();
    Q_EMIT preferDiscreteGPUChanged();
    Q_EMIT pauseHiddenChanged();
    Q_EMIT pauseOnBatChanged();
    Q_EMIT audioVisualizationChanged();
    Q_EMIT audioBackendChanged();
    Q_EMIT audioSourceChanged();
    Q_EMIT wpeCompilePathChanged();
    Q_EMIT wasmCCPathChanged();
    Q_EMIT glslcPathChanged();
    Q_EMIT wpeAssetsPathChanged();
    Q_EMIT wpeAutoImportChanged();
    Q_EMIT wpeLibraryPathChanged();
}

void OptionsManager::onWallpaperdPathChanged() {
    settings.setValue("wallpaperdPath", mWallpaperdPath);
}

void OptionsManager::onVSyncChanged() {
    settings.setValue("vSync", mVSync);
}

void OptionsManager::onFPSLimitChanged() {
    settings.setValue("fpsLimit", mFPSLimit);
}

void OptionsManager::onPreferDiscreteGPUChanged() {
    settings.setValue("preferDiscreteGPU", mPreferDiscreteGPU);
}

void OptionsManager::onPauseHiddenChanged() {
    settings.setValue("pauseHidden", mPauseHidden);
}

void OptionsManager::onPauseOnBatChanged() {
    settings.setValue("pauseOnBat", mPauseOnBat);
}

void OptionsManager::onAudioVisualizationChanged() {
    settings.setValue("audioVisualization", mAudioVisualization);
}

void OptionsManager::onAudioBackendChanged() {
    settings.setValue("audioBackend", mAudioBackend);
}

void OptionsManager::onAudioSourceChanged() {
    settings.setValue("audioSource", mAudioSource);
}

void OptionsManager::onWpeCompilePathChanged() {
    settings.setValue("wpeCompilePath", mWpeCompilePath);
}

void OptionsManager::onWasmCCPathChanged() {
    settings.setValue("wasmCCPath", mWasmCCPath);
}

void OptionsManager::onGlslcPathChanged() {
    settings.setValue("glslcPath", mGlslcPath);
}

void OptionsManager::onWPEAssetsPathChanged() {
    settings.setValue("wpeAssetsPath", mWPEAssetsPath);
}

void OptionsManager::onWPEAutoImportChanged() {
    settings.setValue("wpeAutoImport", mWPEAutoImport);
}

void OptionsManager::onWPELibraryPathChanged() {
    settings.setValue("wpeLibraryPath", mWPELibraryPath);
}
