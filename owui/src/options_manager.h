#ifndef OWUI_OPTIONS_MANAGER_H
#define OWUI_OPTIONS_MANAGER_H

#include <QObject>
#include <QSettings>

class OptionsManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString wallpaperdPath MEMBER mWallpaperdPath NOTIFY wallpaperdPathChanged)
    Q_PROPERTY(bool vSync MEMBER mVSync NOTIFY vSyncChanged)
    Q_PROPERTY(int fpsLimit MEMBER mFPSLimit NOTIFY fpsLimitChanged)
    Q_PROPERTY(bool preferDiscreteGPU MEMBER mPreferDiscreteGPU NOTIFY preferDiscreteGPUChanged)
    Q_PROPERTY(bool pauseHidden MEMBER mPauseHidden NOTIFY pauseHiddenChanged)
    Q_PROPERTY(bool pauseOnBat MEMBER mPauseOnBat NOTIFY pauseOnBatChanged)
    Q_PROPERTY(bool audioVisualization MEMBER mAudioVisualization NOTIFY audioVisualizationChanged)
    Q_PROPERTY(int audioBackend MEMBER mAudioBackend NOTIFY audioBackendChanged)
    Q_PROPERTY(QString audioSource MEMBER mAudioSource NOTIFY audioSourceChanged)
    Q_PROPERTY(QString wpeCompilePath MEMBER mWpeCompilePath NOTIFY wpeCompilePathChanged)
    Q_PROPERTY(QString wasmCCPath MEMBER mWasmCCPath NOTIFY wasmCCPathChanged)
    Q_PROPERTY(QString glslcPath MEMBER mGlslcPath NOTIFY glslcPathChanged)
    Q_PROPERTY(QString wpeAssetsPath MEMBER mWPEAssetsPath NOTIFY wpeAssetsPathChanged)
    Q_PROPERTY(bool wpeAutoImport MEMBER mWPEAutoImport NOTIFY wpeAutoImportChanged)
    Q_PROPERTY(QString wpeLibraryPath MEMBER mWPELibraryPath NOTIFY wpeLibraryPathChanged)

public:
    OptionsManager(QObject* parent = nullptr);

public Q_SLOTS:
    void onWallpaperdPathChanged();
    void onVSyncChanged();
    void onFPSLimitChanged();
    void onPreferDiscreteGPUChanged();
    void onPauseHiddenChanged();
    void onPauseOnBatChanged();
    void onAudioVisualizationChanged();
    void onAudioBackendChanged();
    void onAudioSourceChanged();
    void onWpeCompilePathChanged();
    void onWasmCCPathChanged();
    void onGlslcPathChanged();
    void onWPEAssetsPathChanged();
    void onWPEAutoImportChanged();
    void onWPELibraryPathChanged();

Q_SIGNALS:
    void wallpaperdPathChanged();
    void vSyncChanged();
    void fpsLimitChanged();
    void preferDiscreteGPUChanged();
    void pauseHiddenChanged();
    void pauseOnBatChanged();
    void audioVisualizationChanged();
    void audioBackendChanged();
    void audioSourceChanged();
    void wpeCompilePathChanged();
    void wasmCCPathChanged();
    void glslcPathChanged();
    void wpeAssetsPathChanged();
    void wpeAutoImportChanged();
    void wpeLibraryPathChanged();

private:
    QSettings settings;

    QString mWallpaperdPath;
    bool mVSync;
    int mFPSLimit;
    bool mPreferDiscreteGPU;
    bool mPauseHidden;
    bool mPauseOnBat;
    bool mAudioVisualization;
    int mAudioBackend;
    QString mAudioSource;
    QString mWpeCompilePath;
    QString mWasmCCPath;
    QString mGlslcPath;
    QString mWPEAssetsPath;
    bool mWPEAutoImport;
    QString mWPELibraryPath;
};

#endif
