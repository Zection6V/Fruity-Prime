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
    static final String MAIN_QML = "qrc:/qt/qml/FruityPrime/Ui/Main.qml";

    public LauncherView(Context context) {
        this(context, MAIN_QML);
    }
    /** Another QML source: the startup gate's fault injection, debug builds only. */
    public LauncherView(Context context, String qmlSource) {
        super(context, qmlSource, "FruityPrime");
        setFocusable(true);
        setFocusableInTouchMode(true);
    }
}
