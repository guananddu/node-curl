#include "curl.h"
#include <iostream>
#include <iterator>
#include <utility>
#include <string.h>
#include <unistd.h>

Request::Request ()
    : curl_ (curl_easy_init ()),
      read_pos_ (0)
{
    curl_easy_setopt (curl_, CURLOPT_READFUNCTION, read_data);
    curl_easy_setopt (curl_, CURLOPT_READDATA, this);
    curl_easy_setopt (curl_, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt (curl_, CURLOPT_WRITEDATA, this);
}

Request::~Request () {
    if (curl_) curl_easy_cleanup (curl_);
    std::cerr << ("Request destructor");
}

Handle<Value> Request::New (Handle<Object> options, bool raw) {
    HandleScope scope;

    Handle<Object> handle = NewTemplate ()->NewInstance ();
    Request *request = new Request ();
    request->Wrap (handle);

    // Set options
    Handle<Value> url    = options->Get (String::New ("url"));
    Handle<Value> method = options->Get (String::New ("method"));
    // options.url
    curl_easy_setopt (request->curl_, CURLOPT_URL, *String::Utf8Value (url));
    // options.method
    if (!strcasecmp ("POST", *String::AsciiValue (method)))
        curl_easy_setopt (request->curl_, CURLOPT_POST, 1);

    return scope.Close (handle);
}

// request.write(chunk)
Handle<Value> Request::write (const Arguments& args) {
    HandleScope scope;

    if (args.Length () != 1 && !args[0]->IsString ())
        return THROW_BAD_ARGS;

    Request *request = Unwrap<Request> (args.Holder ());
    String::Utf8Value chunk (Handle<String>::Cast (args[0]));

    request->read_buffer_.insert (request->read_buffer_.end (),
                                  *chunk,
                                  *chunk + chunk.length ());

    return Undefined ();
}

// request.end([chunk])
Handle<Value> Request::end (const Arguments& args) {
    HandleScope scope;

    if (args.Length () > 1)
        return THROW_BAD_ARGS;

    // Have chunk
    if (args.Length () == 1) {
        if (!args[0]->IsString ())
            return THROW_BAD_ARGS;

        Request::write (args);
    }

    Request *request = Unwrap<Request> (args.Holder ());

    // Must set file size
    curl_easy_setopt (request->curl_, CURLOPT_POSTFIELDSIZE, request->read_buffer_.size ());
    // Send them all!
    CURLcode res = curl_easy_perform (request->curl_);
    if (CURLE_OK != res) {
        return ThrowException (Exception::Error (
                    String::New (curl_easy_strerror (res))));
    }

    Handle<String> result = String::New (&request->write_buffer_[0],
                                         request->write_buffer_.size ());
    return scope.Close (result);
}

// request.endFile(filename)
Handle<Value> Request::endFile (const Arguments& args) {

    return Undefined ();
}

Handle<ObjectTemplate> Request::NewTemplate () {
    HandleScope scope;

    Handle<ObjectTemplate> tpl = ObjectTemplate::New ();
    tpl->SetInternalFieldCount (1);
    NODE_SET_METHOD (tpl , "write" , Request::write);
    NODE_SET_METHOD (tpl , "end"   , Request::end);

    return scope.Close (tpl);
}

size_t Request::read_data (void *ptr, size_t size, size_t nmemb, void *userdata) {
    Request *request = static_cast<Request*> (userdata);

    // How many data to write
    size_t need = size * nmemb;
    size_t leaved = request->read_buffer_.size () - request->read_pos_;
    size_t to_write = std::min (need, leaved);

    if (to_write == 0) {
        return 0;
    }

    // Copy data
    memcpy(ptr, &(request->read_buffer_[request->read_pos_]), to_write);
    request->read_pos_ += to_write;

    return to_write;
}

size_t Request::write_data (void *ptr, size_t size, size_t nmemb, void *userdata) {
    Request *request = static_cast<Request*> (userdata);

    // Copy data to buffer
    char *comein = static_cast<char*> (ptr);
    request->write_buffer_.insert (request->write_buffer_.end (),
                                   comein, 
                                   comein + size * nmemb);

    return size * nmemb;
}