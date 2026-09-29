package fr.livetek.fruityprime;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.view.MotionEvent;
import android.view.View;

public final class LauncherView extends View {
    private Bitmap frame;

    public LauncherView(Context context) {
        super(context);
        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        if (frame != null) {
            frame.recycle();
            frame = null;
        }
        if (width > 0 && height > 0) {
            frame = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (frame != null) {
            nativeRender(frame, getWidth(), getHeight());
            canvas.drawBitmap(frame, 0.0f, 0.0f, null);
        }
        postInvalidateOnAnimation();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event == null) {
            return false;
        }
        nativeTouch(event.getActionMasked(), event.getX(), event.getY());
        return true;
    }

    private static native void nativeRender(Bitmap bitmap, int width, int height);
    private static native void nativeTouch(int action, float x, float y);
}
