/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright 2015 Cloudius Systems
 */

#pragma once

#include <concepts>
#include <seastar/http/handlers.hh>
#include <functional>
#include <seastar/json/json_elements.hh>

namespace seastar {

namespace httpd {

/**
 * A request function is a lambda expression that gets only the request
 * as its parameter
 */
typedef std::function<sstring(const_req req)> request_function;

/**
 * A handle function is a lambda expression that gets request and reply
 */
typedef std::function<sstring(const_req req, http::reply&)> handle_function;

/**
 * A json request function is a lambda expression that gets only the request
 * as its parameter and return a json response.
 * Using the json response is done implicitly.
 */
typedef std::function<json::json_return_type(const_req req)> json_request_function;

/**
 * A future_json_function is a function that returns a future json reponse.
 * Similiar to the json_request_function, using the json reponse is done
 * implicitly.
 */
typedef std::function<
        future<json::json_return_type>(std::unique_ptr<http::request> req)> future_json_function;

typedef std::function<
        future<std::unique_ptr<http::reply>>(std::unique_ptr<http::request> req,
                std::unique_ptr<http::reply> rep)> future_handler_function;
/**
 * The function handler get a lambda expression in the constructor.
 * it will call that expression to get the result
 * This is suited for very simple handlers
 *
 */
class function_handler : public handler_base {
public:

    function_handler(const handle_function & f_handle, const sstring& type)
            : _f_handle(
                    [f_handle](std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
                        rep->_content += f_handle(*req.get(), *rep.get());
                        return make_ready_future<std::unique_ptr<http::reply>>(std::move(rep));
                    }), _type(type) {
    }

    function_handler(const future_handler_function& f_handle, const sstring& type)
        : _f_handle(f_handle), _type(type) {
    }

    function_handler(const request_function & _handle, const sstring& type)
            : _f_handle(
                    [_handle](std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
                        rep->_content += _handle(*req.get());
                        return make_ready_future<std::unique_ptr<http::reply>>(std::move(rep));
                    }), _type(type) {
    }

    function_handler(const json_request_function& _handle)
            : _f_handle(
                    [_handle](std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
                        return write_json_reply(std::move(rep), _handle(*req.get()));
                    }), _type("json") {
    }

    function_handler(const future_json_function& _handle)
            : _f_handle(
                    [_handle](std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
                        return _handle(std::move(req)).then([rep = std::move(rep)](json::json_return_type&& res) mutable {
                            return write_json_reply(std::move(rep), std::move(res));
                        });
                    }), _type("json") {
    }

    function_handler(const function_handler&) = default;

    future<std::unique_ptr<http::reply>> handle(const sstring& path,
            std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) override {
        return _f_handle(std::move(req), std::move(rep)).then(
                [this](std::unique_ptr<http::reply> rep) {
                    rep->done(_type);
                    return make_ready_future<std::unique_ptr<http::reply>>(std::move(rep));
                });
    }

private:
    // send the json payload of result to reply, return the reply pointer
    static future<std::unique_ptr<http::reply>> write_json_reply(
        std::unique_ptr<http::reply>&& reply,
        std::same_as<json::json_return_type> auto&& result) {
        if (result._body_writer) {
            reply->write_body("json", std::move(result._body_writer));
        } else {
            reply->_content += result._res;
        }
        return make_ready_future<std::unique_ptr<http::reply>>(std::move(reply));
    }

protected:
    future_handler_function _f_handle;
    sstring _type;
};

}

}
