/* TorcConnection
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
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

/*jslint browser,devel,white,this */
/*global TorcSubscription,TorcWebsocket,Visibility */

var TorcConnection = function ($, torc, statusChanged) {
    "use strict";

    var socket = undefined;
    var subscriptions = {};
    var defaultServiceList = { services: { path: torc.ServicesPath } };
    var serviceList = defaultServiceList;
    var returnFormats = undefined;
    var websocketprotocols = undefined;
    var that = this;

    this.call = function(serviceName, method, params, success, failure) {
        // call to a subscribed service
        if (socket !== undefined && subscriptions[serviceName] && subscriptions[serviceName].methods[method]) {
            socket.call(serviceList[serviceName].path + method, params, success, failure);
            return;
        // just a call, no subscription
        } else if (socket !== undefined && serviceList.hasOwnProperty(serviceName)) {
            socket.call(serviceList[serviceName].path + method, params, success, failure);
            return;
        }
    };

    this.unsubscribe = function (serviceName) {
        if (subscriptions[serviceName]) {
            subscriptions[serviceName].subscription.unsubscribe();
            delete subscriptions[serviceName];
        }
    };

    function subscriptionChanged(name, version, methods, properties) {
        // is this a known service subscription
        if (!subscriptions[name]) {
            return;
        }

        // there was an error subscribing
        if (version === undefined) {
            if (typeof subscriptions[name].subscriptionChanges === 'function') { subscriptions[name].subscriptionChanges(); }
            return;
        }

        // loop through the requested properties and listen for updates
        if (typeof properties === 'object' && $.isArray(subscriptions[name].properties)) {
            subscriptions[name].properties.forEach(function(element) {
                if (properties.hasOwnProperty(element)) {
                    subscriptions[name].subscription.listen(element, subscriptions[name].propertyChanges);
                }});
        }

        // save the methods for validating calls
        subscriptions[name].methods = methods;

        // and notifiy subscriber
        if (typeof subscriptions[name].subscriptionChanges === 'function') {
            subscriptions[name].subscriptionChanges(version, methods, properties, name);
        }
    }

    this.subscribe = function (serviceName, properties, propertyChanges, subscriptionChanges) {
        // is this a known service
        if (!serviceList.hasOwnProperty(serviceName)) {
            if (typeof subscriptionChanges === 'function') { subscriptionChanges(); }
            return;
        }

        // avoid double subscriptions
        if (subscriptions[serviceName]) {
            if (typeof subscriptionChanges === 'function') { subscriptionChanges(); }
            return;
        }

        // actually subscribe
        subscriptions[serviceName] = {
            properties: properties,
            propertyChanges: propertyChanges,
            subscriptionChanges: subscriptionChanges,
            subscription: new TorcSubscription(socket, serviceName, serviceList[serviceName].path, subscriptionChanged)
        };
    };

    this.getServiceList = function () {
        return serviceList;
    };

    this.getReturnFormats = function () {
        return returnFormats;
    };

    this.getWebSocketProtocols = function () {
        return websocketprotocols;
    };

    this.getServiceMethods = function (service) {
        if (subscriptions.hasOwnProperty(service) && typeof subscriptions[service] === 'object') {
            if (subscriptions[service].hasOwnProperty('methods') && typeof subscriptions[service].methods === 'object') {
                return subscriptions[service].methods; } }
        return undefined;
    };

    // N.B. this.properties is the list of properties the subcsciber is interested in.
    // the subscription has the complete list
    this.getServiceProperties = function (service) {
        if (subscriptions.hasOwnProperty(service) && typeof subscriptions[service] === 'object') {
            return subscriptions[service].subscription.getProperties();
        }
        return undefined;
    };

    function serviceListChanged(ignore, value) {
        // there is currently no service that starts or stops other than at startup/shutdown. So this list
        // shouldn't currently change and the subscriber list will be updated if a new socket is used.
        serviceList = value;
    }

    function serviceSubscriptionChanged(version, ignore, properties) {
        if (version !== undefined && typeof properties === 'object' && properties.hasOwnProperty('serviceList') &&
            properties.serviceList.hasOwnProperty('value')) {
            serviceListChanged('serviceList', properties.serviceList.value);
            returnFormats = properties.returnFormats.value;
            websocketprotocols = properties.webSocketProtocols.value;
            statusChanged(torc.SocketReady);
            return;
        }

        serviceListChanged('serviceList', []);
    }

    function connected() {
        that.subscribe('services', ['serviceList'], serviceListChanged, serviceSubscriptionChanged);
    }

    function disconnected() {
        // notify subscribers that they have been disconnected and delete subscription
        Object.getOwnPropertyNames(subscriptions).forEach(function (element) {
            if (typeof subscriptions[element].subscriptionChanges === 'function') { subscriptions[element].subscriptionChanges(); }
            delete subscriptions[element];
        });

        // clear state and schedule reconnect
        socket = undefined;
        serviceList = defaultServiceList;
    }

    function socketStatusChanged (status) {
        // notify the ui
        statusChanged(status);

        if (status === torc.SocketNotConnected) {
            disconnected();
        } else if (status === torc.SocketConnected) {
            connected();
        }
    }

    function connect() {
        socket = new TorcWebsocket($, torc, socketStatusChanged);
    }

    connect();

    // continually try to reconnect if the window is visible
    Visibility.every(2000, function () {
        if (socket === undefined) {
            connect();
        }});
    // and disconnect every time it is hidden
    Visibility.change(function (ignore, state) {
        if (state === 'hidden' && socket !== undefined) {
                socket.stop();
            }
        });
};
