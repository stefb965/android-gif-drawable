#include "gif.h"
#include <sys/eventfd.h>
#include <GLES2/gl2.h>

typedef struct {
	struct pollfd eventPollFd;
	void *frameBuffer;
	pthread_t slurpThread;
} TexImageDescriptor;

__unused JNIEXPORT void JNICALL
Java_pl_droidsonroids_gif_GifInfoHandle_drawTexImage(JNIEnv *__unused unused, jclass __unused handleClass, jlong gifInfo) {
	GifInfo *info = (GifInfo *) gifInfo;
	if (info == NULL || info->frameBufferDescriptor == NULL) {
		return;
	}
	TexImageDescriptor *texImageDescriptor = info->frameBufferDescriptor;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei) info->gifFilePtr->SWidth, (GLsizei) info->gifFilePtr->SHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
	             texImageDescriptor->frameBuffer);
}

static void *slurp(void *pVoidInfo) {
	GifInfo *info = pVoidInfo;
	while (1) {
		long renderStartTime = getRealTime();
		DDGifSlurp(info, true);
		TexImageDescriptor *texImageDescriptor = info->frameBufferDescriptor;
		if (info->currentIndex == 0)
			prepareCanvas(texImageDescriptor->frameBuffer, info);
		const uint_fast32_t frameDuration = getBitmap((argb *) texImageDescriptor->frameBuffer, info);

		const long invalidationDelayMillis = calculateInvalidationDelay(info, renderStartTime, frameDuration);
		int pollResult = TEMP_FAILURE_RETRY(poll(&texImageDescriptor->eventPollFd, 1, (int) invalidationDelayMillis));
		if (pollResult > 0) {
			POLL_TYPE eftd_ctr;
			ssize_t bytesRead = TEMP_FAILURE_RETRY(read(texImageDescriptor->eventPollFd.fd, &eftd_ctr, POLL_TYPE_SIZE));
			if (bytesRead != POLL_TYPE_SIZE) {
				throwException(getEnv(), RUNTIME_EXCEPTION_ERRNO, "Could not read from eventfd ");
			}
			break;
		}
		else if (pollResult < 0) {
			throwException(getEnv(), RUNTIME_EXCEPTION_ERRNO, "Could not poll on eventfd ");
			break;
		}
	}
	DetachCurrentThread();
	return NULL;
}

__unused JNIEXPORT void JNICALL
Java_pl_droidsonroids_gif_GifInfoHandle_startRenderThread(JNIEnv *env, jclass __unused handleClass, jlong gifInfo) {
	GifInfo *info = (GifInfo *) gifInfo;
	if (info == NULL) {
		return;
	}
	TexImageDescriptor *texImageDescriptor = info->frameBufferDescriptor;
	if (texImageDescriptor == NULL) {
		texImageDescriptor = malloc(sizeof(TexImageDescriptor));
		if (!texImageDescriptor) {
			throwException(env, OUT_OF_MEMORY_ERROR, OOME_MESSAGE);
			return;
		}
		texImageDescriptor->frameBuffer = malloc(info->gifFilePtr->SWidth * info->gifFilePtr->SHeight * sizeof(argb));
		if (!texImageDescriptor->frameBuffer) {
			free(texImageDescriptor);
			throwException(env, OUT_OF_MEMORY_ERROR, OOME_MESSAGE);
			return;
		}
		texImageDescriptor->eventPollFd.events = POLL_IN;
		texImageDescriptor->eventPollFd.fd = eventfd(0, 0);
		if (texImageDescriptor->eventPollFd.fd == -1) {
			free(texImageDescriptor);
			throwException(env, RUNTIME_EXCEPTION_ERRNO, "Could not create eventfd ");
			return;
		}
		info->frameBufferDescriptor = texImageDescriptor;
	}
	info->stride = (int32_t) info->gifFilePtr->SWidth;
	if (pthread_create(&texImageDescriptor->slurpThread, NULL, slurp, info) != 0) {
		throwException(env, RUNTIME_EXCEPTION_ERRNO, "Slurp thread creation failed ");
	}
}

__unused JNIEXPORT void JNICALL
Java_pl_droidsonroids_gif_GifInfoHandle_stopRenderThread(JNIEnv *env, jclass __unused handleClass, jlong gifInfo) {
	GifInfo *info = (GifInfo *) (intptr_t) gifInfo;
	if (info == NULL || info->frameBufferDescriptor == NULL) {
		return;
	}
	TexImageDescriptor *texImageDescriptor = info->frameBufferDescriptor;
	POLL_TYPE eftd_ctr;
	ssize_t bytesWritten = TEMP_FAILURE_RETRY(write(texImageDescriptor->eventPollFd.fd, &eftd_ctr, POLL_TYPE_SIZE));
	if (bytesWritten != POLL_TYPE_SIZE) {
		throwException(env, RUNTIME_EXCEPTION_ERRNO, "Could not write to eventfd ");
	}
	errno = pthread_join(texImageDescriptor->slurpThread, NULL);
	if (errno != 0) {
		throwException(env, RUNTIME_EXCEPTION_ERRNO, "Slurp thread join failed ");
	}
	if (close(texImageDescriptor->eventPollFd.fd) != 0 && errno != EINTR) {
		throwException(env, RUNTIME_EXCEPTION_ERRNO, "Eventfd close failed ");
	}
	free(texImageDescriptor->frameBuffer);
	free(texImageDescriptor);
	info->frameBufferDescriptor = NULL;
}

