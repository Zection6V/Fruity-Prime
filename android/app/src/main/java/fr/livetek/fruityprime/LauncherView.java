package fr.livetek.fruityprime;

import android.content.Context;
import org.qtproject.qt.android.QtQuickView;

/** The native game's menus, rendered by Qt Quick into its Android View. */
public final class LauncherView extends QtQuickView {
    @Override public void setVisibility(int visibility) {
        super.setVisibility(visibility);
        if (visibility == VISIBLE) {
            bringToFront();
            requestFocus();
        }
    }
    public LauncherView(Context context) {
        super(context, "qrc:/qt/qml/FruityPrime/Ui/Main.qml", "FruityPrime");
        setFocusable(true);
        setFocusableInTouchMode(true);
    }
}
