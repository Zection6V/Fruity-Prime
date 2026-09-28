package fr.livetek.fruityprime;

import android.content.Context;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

public final class GameSurfaceView extends SurfaceView implements SurfaceHolder.Callback {
    private long nativePeer;

    public GameSurfaceView(Context context) {
        super(context);
        getHolder().addCallback(this);
        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    public void setNativePeer(long peer) {
        nativePeer = peer;
    }

    @Override
    public boolean onCheckIsTextEditor() {
        return nativePeer != 0
                ? nativeOnCheckIsTextEditor(nativePeer)
                : super.onCheckIsTextEditor();
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        return nativePeer != 0
                ? nativeOnCreateInputConnection(nativePeer, outAttrs)
                : super.onCreateInputConnection(outAttrs);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (nativePeer != 0 && nativeOnKeyDown(nativePeer, keyCode, event)) {
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (nativePeer != 0 && nativeOnKeyUp(nativePeer, keyCode, event)) {
            return true;
        }
        return super.onKeyUp(keyCode, event);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (nativePeer != 0 && nativeOnGenericMotionEvent(nativePeer, event)) {
            return true;
        }
        return super.onGenericMotionEvent(event);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        if (nativePeer != 0) {
            nativeSurfaceCreated(nativePeer, holder);
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (nativePeer != 0) {
            nativeSurfaceChanged(nativePeer, holder, format, width, height);
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (nativePeer != 0) {
            nativeSurfaceDestroyed(nativePeer, holder);
        }
    }

    private static native boolean nativeOnCheckIsTextEditor(long peer);
    private static native InputConnection nativeOnCreateInputConnection(long peer, EditorInfo outAttrs);
    private static native boolean nativeOnKeyDown(long peer, int keyCode, KeyEvent event);
    private static native boolean nativeOnKeyUp(long peer, int keyCode, KeyEvent event);
    private static native boolean nativeOnGenericMotionEvent(long peer, MotionEvent event);
    private static native void nativeSurfaceCreated(long peer, SurfaceHolder holder);
    private static native void nativeSurfaceChanged(
            long peer,
            SurfaceHolder holder,
            int format,
            int width,
            int height);
    private static native void nativeSurfaceDestroyed(long peer, SurfaceHolder holder);
}
