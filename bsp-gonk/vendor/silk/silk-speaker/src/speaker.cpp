/**
 * Native implementation of silk-speaker API
 */

#define LOG_TAG "silk-speaker"
#ifdef ANDROID
// #define LOG_NDEBUG 0
#include <utils/Log.h>
#else
#define ALOGV(fmt, args...) fprintf(stderr, LOG_TAG ": " fmt, ##args); fprintf(stderr, "\n");
#define ALOGE ALOGV
#define ALOGD ALOGV
#endif

#include "speaker.h"

Nan::Persistent<Function> Speaker::constructor;

/**
 *
 */
void Speaker::Init(Local<Object> exports) {
  Nan::HandleScope scope;

  // Prepare constructor template
  Local<FunctionTemplate> ctor = Nan::New<FunctionTemplate>(New);
  ctor->SetClassName(Nan::New("Speaker").ToLocalChecked());
  ctor->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(ctor, "open", Open);
  Nan::SetPrototypeMethod(ctor, "write", Write);
  Nan::SetPrototypeMethod(ctor, "close", Close);
  Nan::SetPrototypeMethod(ctor, "setVolume", SetVolume);

  // Constants
  #define CONST_INT(value) \
    Nan::ForceSet(exports, Nan::New(#value).ToLocalChecked(), Nan::New(value), \
      static_cast<PropertyAttribute>(ReadOnly|DontDelete));

  CONST_INT(AUDIO_FORMAT_PCM_8_BIT);
  CONST_INT(AUDIO_FORMAT_PCM_16_BIT);
  CONST_INT(AUDIO_FORMAT_PCM_FLOAT);

  constructor.Reset(ctor->GetFunction());
  exports->Set(Nan::New("Speaker").ToLocalChecked(), ctor->GetFunction());
}

/**
 *
 */
NAN_METHOD(Speaker::New) {
  if (info.IsConstructCall()) {
    // Invoked as constructor: `new Speaker(...)`
    Speaker* obj = new Speaker();
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    // Invoked as plain function `Speaker(...)`, turn into construct call.
    Local<Function> cons = Nan::New<Function>(constructor);
    info.GetReturnValue().Set(cons->NewInstance());
  }
}

/**
 *
 */
Speaker::Speaker():
    mAudioPlayer(NULL),
    gain(GAIN_MAX) {
  ALOGV("Creating instance of speaker");
}

/**
 *
 */
Speaker::~Speaker() {
}

NAN_METHOD(Speaker::Open) {
  SETUP_FUNCTION(Speaker)

  if (info.Length() != 3) {
    JSTHROW("Invalid number of arguments provided");
  }

  int channelCount = info[0]->Int32Value();
  uint32_t sampleRate = info[1]->Uint32Value();
  int audioFormat = info[2]->Int32Value();
  ALOGV("channelCount %d", channelCount);
  ALOGV("sampleRate %u", sampleRate);
  ALOGV("audioFormat %d", audioFormat);

  // Start the audio playback
  self->mAudioPlayer = new AudioPlayer(sampleRate, (audio_format_t) audioFormat,
                                       channelCount);
  self->mAudioPlayer->init();

  // Start with the default volume of max unless user has called the setVolume
  // to set the default volume level
  self->mAudioPlayer->setVolume(self->gain);
}

/**
 * Async worker handler for writing audio data
 */
class WriteAsyncWorker: public Nan::AsyncWorker {
public:
  WriteAsyncWorker(Nan::Callback *callback, Speaker* speaker, char *buffer, int len):
    Nan::AsyncWorker(callback),
    speaker(speaker),
    buffer(buffer),
    len(len),
    written(0) {
  }

  void Execute() {
    // Write audio data
    written = speaker->mAudioPlayer->write((const void*) buffer, len);
  }

  void HandleOKCallback() {
    Local<Value> argv[] = {
      Nan::New(written)
    };
    callback->Call(1, argv);
  }

private:
  Speaker *speaker;
  char *buffer;
  int len;
  int written;
};

NAN_METHOD(Speaker::Write) {
  SETUP_FUNCTION(Speaker)

  if (info.Length() != 3) {
     JSTHROW("Invalid number of arguments provided");
  }

  char *buffer = UnwrapPointer(info[0]);
  int len = info[1]->Int32Value();
  REQ_FUN_ARG(2, cb);
  ALOGV("Received %d bytes to be written", len);

  // Write audio data aysnc
  Nan::Callback *callback = new Nan::Callback(cb.As<Function>());
  Nan::AsyncQueueWorker(new WriteAsyncWorker(callback, self, buffer, len));
}

NAN_METHOD(Speaker::SetVolume) {
  SETUP_FUNCTION(Speaker)

  if (info.Length() != 1) {
    JSTHROW("Invalid number of arguments provided");
  }

  self->gain = info[0]->NumberValue();

  if (self->mAudioPlayer != NULL) {
    self->mAudioPlayer->setVolume(self->gain);
  }
}

NAN_METHOD(Speaker::Close) {
  SETUP_FUNCTION(Speaker)

  // Stop the underlying audio track. This will stop sending
  // data to the mixer and discard any pending buffers that the
  // track holds
  self->mAudioPlayer->stop();
}

NODE_MODULE(speaker, Speaker::Init);
