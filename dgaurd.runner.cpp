// 인코딩과 디코딩을 위한 백그라운드 작업을 수행하는 모듈입니다.
// 각 요청은 Promise로 핸들되어야 하고, 이 요청들은 스레드 풀에서 처리되어야 합니다.

#include <node.h>
#include <uv.h>


#include <iostream>
#include <thread>
#include <queue>
#include <string>

namespace dguard

{
    using v8::FunctionCallbackInfo;
    using v8::Isolate;
    using v8::String;
    using v8::Local;
    using v8::Object;
    using v8::Value;
    using v8::Boolean;
    using v8::Promise;

    enum CryptoRequestType {
        ENCODE,
        DECODE
    };

    struct CryptoRequest {
        uv_work_t request;
        char* input;
        CryptoRequestType type;
        char* result;
        v8::Persistent<Promise::Resolver> promise;
        std::string error;
    };

    std::atomic<bool> running = true;
    std::queue<CryptoRequest*> requests;
    std::thread crypto_thread;

    void encoder(uv_work_t* req) {
        CryptoRequest* request = static_cast<CryptoRequest*>(req->data);

        std::this_thread::sleep_for(std::chrono::seconds(2));

        request->result = "encoded";
    }

    void decoder(uv_work_t* req) {
        CryptoRequest* request = static_cast<CryptoRequest*>(req->data);

        std::this_thread::sleep_for(std::chrono::seconds(2));

        request->result = "decoded";
    }

    void afterCryptoWork(uv_work_t* req, int status) {
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        v8::HandleScope scope(isolate);

        CryptoRequest* request = static_cast<CryptoRequest*>(req->data);

        Local<Promise::Resolver> resolver = 
            Local<Promise::Resolver>::New(isolate, request->promise);

        if (status == UV_ECANCELED) {
            request->error = "Operation was cancelled";
        }

        if (request->error.empty()) {
            Local<Value> result = String::NewFromUtf8(
                isolate,
                request->result
            ).ToLocalChecked();

            resolver->Resolve(
                isolate->GetCurrentContext(),
                result
            );
        } else {
            Local<Value> error = String::NewFromUtf8(
                isolate,
                request->error.c_str()
            ).ToLocalChecked();

            resolver->Reject(
                isolate->GetCurrentContext(),
                error
            );
        }

        request->promise.Reset();
        delete request;
    }

    void Start(const FunctionCallbackInfo<Value>& args) {
        Isolate* isolate = args.GetIsolate();

        running = true;

        args.GetReturnValue().Set(
            String::NewFromUtf8(
                isolate,
                "Background task started"
            ).ToLocalChecked()
        );
    }

    void Stop(const FunctionCallbackInfo<Value>& args) {
        Isolate* isolate = args.GetIsolate();
        
        running = false;

        args.GetReturnValue().Set(
            String::NewFromUtf8(
                isolate,
                "Background task stopped"
            ).ToLocalChecked()
        );
    }

    void isRunning(const FunctionCallbackInfo<Value>& args) {
        Isolate* isolate = args.GetIsolate();
        
        args.GetReturnValue().Set(
            Boolean::New(isolate, running)
        );
    }

    void Encode(const FunctionCallbackInfo<Value>& args) {
        Isolate* isolate = args.GetIsolate();
        
        if (!running) {
            isolate->ThrowException(
                String::NewFromUtf8(
                    isolate,
                    "Background task is not running"
                ).ToLocalChecked()
            );
            return;
        }

        // needs to have one argument
        if (args.Length() != 1) {
            isolate->ThrowException(
                String::NewFromUtf8(
                    isolate,
                    "Wrong number of arguments"
                ).ToLocalChecked()
            );
            return;
        }

        // argument needs to be a string
        if (!args[0]->IsString()) {
            isolate->ThrowException(
                String::NewFromUtf8(
                    isolate,
                    "Wrong arguments"
                ).ToLocalChecked()
            );
            return;
        }

        Local<Promise::Resolver> resolver = 
            Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();
        Local<String> input = args[0].As<String>();

        CryptoRequest* request = new CryptoRequest();
        request->input = *String::Utf8Value(isolate, input);
        request->type = ENCODE;
        request->request.data = request;

        request->promise.Reset(isolate, resolver);

        uv_queue_work(
            uv_default_loop(),
            &request->request,
            encoder,
            afterCryptoWork
        );

        args.GetReturnValue().Set(resolver->GetPromise());
    }

    void Decode(const FunctionCallbackInfo<Value>& args) {
        Isolate* isolate = args.GetIsolate();
        
        if (!running) {
            isolate->ThrowException(
                String::NewFromUtf8(
                    isolate,
                    "Background task is not running"
                ).ToLocalChecked()
            );
            return;
        }

        // needs to have one argument
        if (args.Length() != 1) {
            isolate->ThrowException(
                String::NewFromUtf8(
                    isolate,
                    "Wrong number of arguments"
                ).ToLocalChecked()
            );
            return;
        }

        // argument needs to be a string
        if (!args[0]->IsString()) {
            isolate->ThrowException(
                String::NewFromUtf8(
                    isolate,
                    "Wrong arguments"
                ).ToLocalChecked()
            );
            return;
        }

        Local<Promise::Resolver> resolver = Promise::Resolver::New(
            isolate->GetCurrentContext()
        ).ToLocalChecked();
        Local<String> input = args[0].As<String>();

        CryptoRequest* request = new CryptoRequest();
        request->input = *String::Utf8Value(isolate, input);
        request->type = DECODE;
        request->request.data = request;

        request->promise.Reset(isolate, resolver);

        uv_queue_work(
            uv_default_loop(),
            &request->request,
            decoder,
            afterCryptoWork
        );

        args.GetReturnValue().Set(resolver->GetPromise());
    }

    void Init(Local<Object> exports) {
        NODE_SET_METHOD(exports, "start", Start);
        NODE_SET_METHOD(exports, "stop", Stop);
        NODE_SET_METHOD(exports, "isRunning", isRunning);
        NODE_SET_METHOD(exports, "encode", Encode);
        NODE_SET_METHOD(exports, "decode", Decode);
    }

    NODE_MODULE(NODE_GYP_MODULE_NAME, Init)
}