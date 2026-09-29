package fr.livetek.fruityprime;

import android.content.Context;
import android.graphics.Canvas;
import android.view.MotionEvent;
import android.view.View;

public final class TouchOverlayView extends View {
    private long nativePeer;

    public TouchOverlayView(Context context) {
        super(context);
        setWillNotDraw(false);
        setBackgroundColor(0x00000000);
    }

    public void setNativePeer(long peer) {
        nativePeer = peer;
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        if (nativePeer != 0) {
            nativeOnSizeChanged(nativePeer, width, height, oldWidth, oldHeight);
        } else {
            super.onSizeChanged(width, height, oldWidth, oldHeight);
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (nativePeer != 0) {
            nativeOnDraw(nativePeer, canvas);
        } else {
            super.onDraw(canvas);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (nativePeer != 0) {
            return nativeOnTouchEvent(nativePeer, event);
        }
        return super.onTouchEvent(event);
    }

    private static native void nativeOnSizeChanged(
            long peer,
            int width,
            int height,
            int oldWidth,
            int oldHeight);
    private static native void nativeOnDraw(long peer, Canvas canvas);
    private static native boolean nativeOnTouchEvent(long peer, MotionEvent event);
}
