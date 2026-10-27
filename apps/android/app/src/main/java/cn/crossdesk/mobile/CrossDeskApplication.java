package cn.crossdesk.mobile;

import android.app.Application;

/** Initialize localization before any activity, background service or model. */
public final class CrossDeskApplication extends Application {
    @Override public void onCreate() {
        super.onCreate();
        L10n.initialize(this);
    }
}
