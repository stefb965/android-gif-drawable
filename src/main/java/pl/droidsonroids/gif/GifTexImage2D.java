package pl.droidsonroids.gif;

import java.io.IOException;

public class GifTexImage2D {
	private final GifInfoHandle mGifInfoHandle;

	/**
	 * Constructs new GifTexImage2D
	 *
	 * @param inputSource source
	 * @throws IOException when creation fails
	 */
	public GifTexImage2D(final InputSource inputSource) throws IOException {
		mGifInfoHandle = inputSource.open();
	}

	public void draw() {
		mGifInfoHandle.drawTexImage();
	}

	public void startRenderThread() {
		mGifInfoHandle.startRenderThread();
	}

	public void stopRenderThread() {
		mGifInfoHandle.stopRenderThread();
	}

	public void recycle() {
		stopRenderThread();
		mGifInfoHandle.recycle();
	}

	@Override
	protected void finalize() throws Throwable {
		try {
			recycle();
		} finally {
			super.finalize();
		}
	}
}
