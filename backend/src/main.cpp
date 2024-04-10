#include "afv-native/event.h"
#include <absl/strings/match.h>
#include <afv-native/atcClientWrapper.h>
#include <memory>
#include <napi.h>
#include <string>

#define VERSION "1.0.0"

static std::unique_ptr<afv_native::api::atcClient> mClient =
    std::make_unique<afv_native::api::atcClient>("TrackAudio");

static Napi::ThreadSafeFunction callbackRef;
static bool callbackAvailable = false;

Napi::Array GetAudioApis(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  Napi::Array arr = Napi::Array::New(env);

  for (const auto [apiId, apiName] : mClient->GetAudioApis()) {
    Napi::Object obj = Napi::Object::New(env);
    obj.Set("id", apiId);
    obj.Set("name", apiName);
    arr.Set(arr.Length(), obj);
  }

  return arr;
}

Napi::Array GetAudioInputDevices(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  int apiId = info[0].As<Napi::Number>().Int32Value();
  Napi::Array arr = Napi::Array::New(env);

  for (const auto [deviceId, deviceName, isDefault] :
       mClient->GetAudioInputDevices(apiId)) {
    Napi::Object obj = Napi::Object::New(env);
    obj.Set("id", deviceId);
    obj.Set("name", deviceName);
    obj.Set("isDefault", isDefault);
    arr.Set(arr.Length(), obj);
  }

  return arr;
}

Napi::Array GetAudioOutputDevices(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  int apiId = info[0].As<Napi::Number>().Int32Value();
  Napi::Array arr = Napi::Array::New(env);

  for (const auto [deviceId, deviceName, isDefault] :
       mClient->GetAudioOutputDevices(apiId)) {
    Napi::Object obj = Napi::Object::New(env);
    obj.Set("id", deviceId);
    obj.Set("name", deviceName);
    obj.Set("isDefault", isDefault);
    arr.Set(arr.Length(), obj);
  }

  return arr;
}

Napi::Boolean Connect(const Napi::CallbackInfo &info) {
  if (mClient->IsVoiceConnected()) {
    return Napi::Boolean::New(info.Env(), false);
  }
  Napi::Env env = info.Env();
  auto cid = info[0].As<Napi::String>().Utf8Value();
  auto password = info[1].As<Napi::String>().Utf8Value();
  auto callsign = info[2].As<Napi::String>().Utf8Value();

  mClient->SetCallsign(callsign);
  mClient->SetCredentials(cid, password);
  mClient->SetClientPosition(33.9424964, -118.4080486, 150, 150);
  return Napi::Boolean::New(env, mClient->Connect());
}

void Disconnect(const Napi::CallbackInfo &info) {
  if (!mClient->IsVoiceConnected()) {
    return;
  }
  mClient->Disconnect();
}

void SetAudioSettings(const Napi::CallbackInfo &info) {
  if (mClient->IsVoiceConnected()) {
    return; // Don't allow changing audio settings while connected
  }
  int apiId = info[0].As<Napi::Number>().Int32Value();
  auto inputDeviceId = info[1].As<Napi::String>().Utf8Value();
  auto headsetOutputDeviceId = info[2].As<Napi::String>().Utf8Value();
  auto speakersOutputDeviceId = info[3].As<Napi::String>().Utf8Value();

  mClient->SetAudioApi(apiId);
  mClient->SetAudioInputDevice(inputDeviceId);
  mClient->SetAudioOutputDevice(headsetOutputDeviceId);
  mClient->SetAudioSpeakersOutputDevice(speakersOutputDeviceId);
}

Napi::Boolean AddFrequency(const Napi::CallbackInfo &info) {
  if (!mClient->IsVoiceConnected()) {
    return Napi::Boolean::New(info.Env(), false);
  }

  int frequency = info[0].As<Napi::Number>().Int32Value();
  auto callsign = info[1].As<Napi::String>().Utf8Value();
  if (mClient->IsFrequencyActive(frequency)) {
    return Napi::Boolean::New(info.Env(), false);
  }

  mClient->AddFrequency(frequency, callsign);
  if (!callsign.empty()) {
    mClient->FetchTransceiverInfo(callsign);
    if (!absl::EndsWith(callsign, "_ATIS")) {
      mClient->SetRx(frequency, true);
    }
  } else {
    mClient->SetRx(frequency, true);
  }

  return Napi::Boolean::New(info.Env(), true);
}

void RemoveFrequency(const Napi::CallbackInfo &info) {
  int frequency = info[0].As<Napi::Number>().Int32Value();
  mClient->RemoveFrequency(frequency);
}

Napi::Boolean SetFrequencyState(const Napi::CallbackInfo &info) {
  if (!mClient->IsVoiceConnected()) {
    return Napi::Boolean::New(info.Env(), false);
  }

  int frequency = info[0].As<Napi::Number>().Int32Value();
  if (!mClient->IsFrequencyActive(frequency)) {
    return Napi::Boolean::New(info.Env(), false);
  }

  bool rx = info[1].As<Napi::Boolean>().Value();
  bool tx = info[2].As<Napi::Boolean>().Value();
  bool xc = info[3].As<Napi::Boolean>().Value();
  bool onSpeaker = info[4].As<Napi::Boolean>().Value();

  mClient->SetRx(frequency, rx);
  mClient->SetTx(frequency, tx);
  mClient->SetXc(frequency, xc);
  mClient->SetOnHeadset(frequency, !onSpeaker);

  return Napi::Boolean::New(info.Env(), true);
}

Napi::Object GetFrequencyState(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  int frequency = info[0].As<Napi::Number>().Int32Value();
  Napi::Object obj = Napi::Object::New(env);

  obj.Set("rx", mClient->GetRxState(frequency));
  obj.Set("tx", mClient->GetTxState(frequency));
  obj.Set("xc", mClient->GetXcState(frequency));
  obj.Set("onSpeaker", !mClient->GetOnHeadset(frequency));

  return obj;
}

void RegisterCallback(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  auto callbackFunction = info[0].As<Napi::Function>();

  // Create a ThreadSafeFunction
  callbackRef = Napi::ThreadSafeFunction::New(
      env, callbackFunction, "trackaudio-afv-res", 0, 1, [](Napi::Env) {});
  callbackAvailable = true;
}

void GetStation(const Napi::CallbackInfo &info) {
  if (!mClient->IsVoiceConnected()) {
    return;
  }
  auto callsign = info[0].As<Napi::String>().Utf8Value();
  mClient->GetStation(callsign);
  mClient->FetchStationVccs(callsign);
}

void RefreshStation(const Napi::CallbackInfo &info) {
  if (!mClient->IsVoiceConnected()) {
    return;
  }
  auto callsign = info[0].As<Napi::String>().Utf8Value();
  mClient->FetchTransceiverInfo(callsign);
}

Napi::String Version(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  return Napi::String::New(env, VERSION);
}

static void CallbackWithError(std::string message) {
  if (!callbackAvailable) {
    return;
  }

  callbackRef.NonBlockingCall(
      [message](Napi::Env env, Napi::Function jsCallback) {
        jsCallback.Call({Napi::String::New(env, "error"),
                         Napi::String::New(env, message),
                         Napi::String::New(env, "")});
      });
}

static void HandleAfvEvents(afv_native::ClientEventType eventType, void *data,
                            void *data2) {
  if (!callbackAvailable) {
    return;
  }

  auto eventId = static_cast<int>(eventType);

  callbackRef.NonBlockingCall([eventId](Napi::Env env,
                                        Napi::Function jsCallback) {
    jsCallback.Call({Napi::String::New(env, std::to_string(eventId)),
                     Napi::String::New(env, ""), Napi::String::New(env, "")});
  });

  if (eventType == afv_native::ClientEventType::VoiceServerConnected) {
    callbackRef.NonBlockingCall([](Napi::Env env, Napi::Function jsCallback) {
      jsCallback.Call({Napi::String::New(env, "VoiceConnected"),
                       Napi::String::New(env, ""), Napi::String::New(env, "")});
    });
  }

  if (eventType == afv_native::ClientEventType::VoiceServerDisconnected) {
    callbackRef.NonBlockingCall([](Napi::Env env, Napi::Function jsCallback) {
      jsCallback.Call({Napi::String::New(env, "VoiceDisconnected"),
                       Napi::String::New(env, ""), Napi::String::New(env, "")});
    });
  }

  if (eventType == afv_native::ClientEventType::StationTransceiversUpdated) {
    if (data == nullptr) {
      return;
    }

    std::string station = *reinterpret_cast<std::string *>(data);
    auto transceiverCount = mClient->GetTransceiverCountForStation(station);
    callbackRef.NonBlockingCall(
        [transceiverCount, station](Napi::Env env, Napi::Function jsCallback) {
          jsCallback.Call(
              {Napi::String::New(env, "StationTransceiversUpdated"),
               Napi::String::New(env, station),
               Napi::String::New(env, std::to_string(transceiverCount))});
        });
  }

  if (eventType == afv_native::ClientEventType::StationDataReceived) {
    if (data == nullptr || data2 == nullptr) {
      return;
    }

    bool found = *reinterpret_cast<bool *>(data);
    if (!found) {
      CallbackWithError("Station not found");
      return;
    }
    auto stationData =
        *reinterpret_cast<std::pair<std::string, unsigned int> *>(data2);
    std::string callsign = stationData.first;
    unsigned int frequency = stationData.second;
    callbackRef.NonBlockingCall(
        [callsign, frequency](Napi::Env env, Napi::Function jsCallback) {
          jsCallback.Call({Napi::String::New(env, "StationDataReceived"),
                           Napi::String::New(env, callsign),
                           Napi::String::New(env, std::to_string(frequency))});
        });
  }

  if (eventType == afv_native::ClientEventType::VccsReceived) {
    if (data == nullptr || data2 == nullptr) {
      return;
    }
    std::map<std::string, unsigned int> stations =
        *reinterpret_cast<std::map<std::string, unsigned int> *>(data2);

    for (const auto &station : stations) {
      const std::string &callsign = station.first;
      const unsigned int frequency = station.second;
      callbackRef.NonBlockingCall(
          [callsign, frequency](Napi::Env env, Napi::Function jsCallback) {
            jsCallback.Call(
                {Napi::String::New(env, "StationDataReceived"),
                 Napi::String::New(env, callsign),
                 Napi::String::New(env, std::to_string(frequency))});
          });
    }
  }

  if (eventType == afv_native::ClientEventType::FrequencyRxBegin) {
    if (data == nullptr) {
      return;
    }

    int frequency = *reinterpret_cast<int *>(data);
    callbackRef.NonBlockingCall(
        [frequency](Napi::Env env, Napi::Function jsCallback) {
          jsCallback.Call({Napi::String::New(env, "FrequencyRxBegin"),
                           Napi::String::New(env, std::to_string(frequency)),
                           Napi::String::New(env, "")});
        });
  }

  if (eventType == afv_native::ClientEventType::FrequencyRxEnd) {
    if (data == nullptr) {
      return;
    }

    int frequency = *reinterpret_cast<int *>(data);
    callbackRef.NonBlockingCall(
        [frequency](Napi::Env env, Napi::Function jsCallback) {
          jsCallback.Call({Napi::String::New(env, "FrequencyRxEnd"),
                           Napi::String::New(env, std::to_string(frequency)),
                           Napi::String::New(env, "")});
        });
  }

  if (eventType == afv_native::ClientEventType::PttOpen) {

    callbackRef.NonBlockingCall([](Napi::Env env, Napi::Function jsCallback) {
      jsCallback.Call({Napi::String::New(env, "PttState"),
                       Napi::String::New(env, "1"),
                       Napi::String::New(env, "")});
    });
  }

  if (eventType == afv_native::ClientEventType::PttClosed) {
    callbackRef.NonBlockingCall([](Napi::Env env, Napi::Function jsCallback) {
      jsCallback.Call({Napi::String::New(env, "PttState"),
                       Napi::String::New(env, "0"),
                       Napi::String::New(env, "")});
    });
  }

  if (eventType == afv_native::ClientEventType::AudioError) {
    CallbackWithError("Error stating audio devices, check your configuration.");
  }

  if (eventType == afv_native::ClientEventType::APIServerError) {
    if (data == nullptr) {
      return;
    }

    auto err = *reinterpret_cast<afv_native::afv::APISessionError *>(data);

    if (err == afv_native::afv::APISessionError::BadPassword ||
        err == afv_native::afv::APISessionError::RejectedCredentials) {
      CallbackWithError("Invalid Credentials");
    }

    if (err == afv_native::afv::APISessionError::ConnectionError) {
      CallbackWithError(
          "API Connection Error, check your internet connection.");
    }

    if (err ==
        afv_native::afv::APISessionError::BadRequestOrClientIncompatible) {
      CallbackWithError("Bad Request or Client Incompatible");
    }

    if (err == afv_native::afv::APISessionError::InvalidAuthToken) {
      CallbackWithError("Invalid Auth Token.");
    }

    if (err == afv_native::afv::APISessionError::AuthTokenExpiryTimeInPast) {
      CallbackWithError(
          "Auth Token has expired, check if your system time is correct.");
    }

    if (err == afv_native::afv::APISessionError::OtherRequestError) {
      CallbackWithError("Unknown Error with AFV API");
    }
  }
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  // Setup afv
  mClient->RaiseClientEvent(
      [](afv_native::ClientEventType eventType, void *data1, void *data2) {
        HandleAfvEvents(eventType, data1, data2);
      });

  exports.Set(Napi::String::New(env, "GetVersion"),
              Napi::Function::New(env, Version));

  exports.Set(Napi::String::New(env, "GetAudioApis"),
              Napi::Function::New(env, GetAudioApis));

  exports.Set(Napi::String::New(env, "GetAudioInputDevices"),
              Napi::Function::New(env, GetAudioInputDevices));

  exports.Set(Napi::String::New(env, "GetAudioOutputDevices"),
              Napi::Function::New(env, GetAudioOutputDevices));

  exports.Set(Napi::String::New(env, "Connect"),
              Napi::Function::New(env, Connect));

  exports.Set(Napi::String::New(env, "Disconnect"),
              Napi::Function::New(env, Disconnect));

  exports.Set(Napi::String::New(env, "SetAudioSettings"),
              Napi::Function::New(env, SetAudioSettings));

  exports.Set(Napi::String::New(env, "AddFrequency"),
              Napi::Function::New(env, AddFrequency));

  exports.Set(Napi::String::New(env, "RemoveFrequency"),
              Napi::Function::New(env, RemoveFrequency));

  exports.Set(Napi::String::New(env, "SetFrequencyState"),
              Napi::Function::New(env, SetFrequencyState));

  exports.Set(Napi::String::New(env, "GetFrequencyState"),
              Napi::Function::New(env, GetFrequencyState));

  exports.Set(Napi::String::New(env, "RegisterCallback"),
              Napi::Function::New(env, RegisterCallback));

  exports.Set(Napi::String::New(env, "GetStation"),
              Napi::Function::New(env, GetStation));

  exports.Set(Napi::String::New(env, "RefreshStation"),
              Napi::Function::New(env, RefreshStation));

  return exports;
}

NODE_API_MODULE(addon, Init)
