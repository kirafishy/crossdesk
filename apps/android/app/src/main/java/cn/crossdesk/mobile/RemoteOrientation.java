package cn.crossdesk.mobile;

import android.app.Activity;
import android.content.pm.ActivityInfo;

/** Applies the selected orientation policy for the whole remote session. */
final class RemoteOrientation {
    private final Activity activity;
    private boolean autoLandscape;

    RemoteOrientation(Activity activity) {
        this.activity = activity;
    }

    void begin(boolean autoLandscape, boolean fullScreenPhone) {
        this.autoLandscape = autoLandscape;
        update(fullScreenPhone);
    }

    boolean autoLandscape() { return autoLandscape; }

    void end() { autoLandscape = false; }

    void update(boolean fullScreenPhone) {
        int orientation = autoLandscape && fullScreenPhone
            ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            : ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR;
        if (activity.getRequestedOrientation() != orientation) activity.setRequestedOrientation(orientation);
    }
}
