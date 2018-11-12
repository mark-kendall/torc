/* TorcWebSocket
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013-16
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

/*jslint browser,devel,white,for,this */
/*global window,MozWebSocket,WebSocket */

var TorcWebsocket = function ($, torc, socketStatusChanged) {
    "use strict";

    // current JSON-RPC Id
    var currentRpcId = 1;
    // current calls
    var currentCalls = [];
    // event handlers for notifications
    var eventHandlers = [];
    // interval timer to check for call expiry/failure
    var expireTimer = null;
    // current status
    var socketStatus = torc.SocketNotConnected;
    // socket
    var socket;

    // update status and notify owner
    function setSocketStatus (Status, Silent) {
        if (typeof Silent === "undefined" && typeof socketStatusChanged === "function") {
            socketStatusChanged(Status);
        }
        socketStatus = Status;
    }

    // clear out any old remote calls (over 60 seconds) for which we did not receive a response
    function expireCalls() {
        var i;
        var callback;
        var timenow = new Date().getTime();

        for (i = currentCalls.length - 1; i >= 0; i -= 1) {
            if ((currentCalls[i].sent + 60000) < timenow) {
                callback = currentCalls[i].failure;
                currentCalls.splice(i, 1);
                if (typeof callback === "function") { callback(); }
            }
        }

        if (currentCalls.length < 1) {
            clearInterval(expireTimer);
            expireTimer = null;
        }
    }

    // add event callback
    this.listen = function (id, method, callback) {
        eventHandlers[method] = { id, callback };
    };

    // make a remote call (public)
    this.call = function (methodToCall, params, successCallback, failureCallback) {
        // create the base call object
        var invocation = {
            jsonrpc: "2.0",
            method: methodToCall
        };

        // add params if supplied
        if (typeof params === "object" && params !== null) {
            invocation.params = params;
        }

        // no callback indicates a notification
        if (typeof successCallback === "function") {
            invocation.id = currentRpcId;
            currentCalls.push({ id: currentRpcId, success: successCallback, failure: failureCallback, sent: new Date().getTime() });
            currentRpcId += 1;

            // wrap the id when it gets large...
            if (currentRpcId > 0x2000000) {
                currentRpcId = 1;
            }

            // start the expire timer if not already running
            if (expireTimer === null) {
                expireTimer = setInterval(expireCalls, 10000);
            }
        }

        socket.send(JSON.stringify(invocation));
    };

    function processResponse (data, error) {
        // this is the result of a successful call
        var id = parseInt(data.id, 10);
        for (var i = 0; i < currentCalls.length; i += 1) {
            if (currentCalls[i].id === id) {
                var callback = error === false ? currentCalls[i].success : currentCalls[i].failure;
                currentCalls.splice(i, 1);
                if (typeof callback === "function") { callback(error === false ? data.result : data); }
                return;
            }
        }
    }

    function processCallback (data) {
        // there is no support for calls to the browser...
        if (data.hasOwnProperty("id")) { return {jsonrpc: "2.0", error: {code: "-32601", message: "Method not found"}, id: data.id}; }
        var method = data.method;
        if (eventHandlers[method]) { eventHandlers[method].callback(eventHandlers[method].id, data.params); }
    }

    function processError (data) {
        if (!data.hasOwnProperty("id")) return;
        processResponse(data, true);
    }

    function processGoodResult (data) {
        if (data.hasOwnProperty("result") && data.hasOwnProperty("id")) { processResponse(data, false); }
        else if (data.hasOwnProperty("method")) { processCallback(data); }
        else if (data.hasOwnProperty("error")) { processError(data); }
    }

    // handle individual responses
    function processResult (data) {
        // we only understand JSON-RPC 2.0
        if (!(data.hasOwnProperty("jsonrpc") && data.jsonrpc === "2.0")) {
            var id = data.hasOwnProperty("id") ? data.id : null;
            return {jsonrpc: "2.0", error: {code: "-32600", message: "Invalid request"}, id};
        }

        processGoodResult(data);
    }

    function connect (token) {
        var url = (window.location.protocol.includes("https") ? "wss://" : "ws://") + window.location.host + "?accesstoken=" + token;
        socket = (typeof MozWebSocket === "function") ? new MozWebSocket(url, "torc.json-rpc") : new WebSocket(url, "torc.json-rpc");

        // socket closed
        socket.onclose = function () {
            setSocketStatus(torc.SocketNotConnected);
        };

        // socket opened
        socket.onopen = function () {
            // connected
            setSocketStatus(torc.SocketConnected);
        };

        // socket message
        socket.onmessage = function (event) {
            var i;
            var result;
            var batchresult;

            // parse the JSON result
            var data = JSON.parse(event.data);

            if ($.isArray(data)) {
                // array of objects (batch)
                batchresult = [];

                for (i = 0; i < data.length; i += 1) {
                    result = processResult(data[i]);
                    if (typeof result === "object") { batchresult.push(result); }
                }

                // a batch call of notifications requires no response
                if (batchresult.length > 0) { socket.send(JSON.stringify(batchresult)); }
            } else if (typeof data === "object") {
                // single object
                result = processResult(data);

                if (typeof result === "object") { socket.send(JSON.stringify(result)); }
            } else {
                socket.send(JSON.stringify({jsonrpc: "2.0", error: {code: "-32700", message: "Parse error"}, id: null}));
            }
        };
    }

    function start () {
        if (socketStatus === torc.SocketConnecting || socketStatus === torc.SocketConnected) { return; }

        setSocketStatus(torc.SocketConnecting);
        // start the connection by requesting a token. If authentication is not required, it will be silently ignored.
        $.ajax({ url: torc.ServicesPath + "GetWebSocketToken",
                 dataType: "json",
                 type: "GET",
                 xhrFields: { withCredentials: true },
                 success(result) { connect(result.accesstoken); },
                 error() { setSocketStatus(torc.SocketNotConnected); }
               });
    }

    this.stop = function () {
        if (socketStatus === torc.SocketConnected || socketStatus === torc.SocketConnecting) {
            setSocketStatus(torc.SocketDisconnecting);
            socket.close();
        }
    };

    // set the initial socket state
    setSocketStatus(socketStatus, false /*don't notify status*/);
    start();
};
