/* TorcStateGraph
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2016
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
/*global */

var TorcStateGraph = function ($, torc) {
    "use strict";

    var torcconnection = null;
    var sensorTypes    = null;

    function deviceChanged(name, value, service) {
        console.log(service + ': ' + name + ':' + value);
    }

    function deviceSubscriptionChanged(version, ignore, properties, service) {
        if (version !== undefined && typeof properties === 'object') {
            $.each(properties, function (key, value) {
                deviceChanged(key, value.value, service);
            });
            return;
        }
    }

    function sensorsChanged (name, value) {
        if (name === 'sensorTypes' && $.isArray(value)) {
            sensorTypes = value;
        } else if (name === 'sensorList' && typeof value === 'object') {
            if ($.isArray(sensorTypes) && sensorTypes.length > 0) {
                $.each(sensorTypes, function (ignore, type) {
                    $.each(value, function (key, sensors) {
                        if (key === type && $.isArray(sensors)) {
                            $.each(sensors, function (ignore, sensor) {
                                torcconnection.subscribe(sensor, ['value'], deviceChanged, deviceSubscriptionChanged);
                            });
                        }
                    });
                });
            }
        }
    }

    function sensorSubscriptionChanged(version, ignore, properties) {
        if (version !== undefined && typeof properties === 'object') {
            // NB update sensorTypes first so that we can iterate over sensorList meaningfully
            sensorsChanged('sensorTypes', properties.sensorTypes.value);
            $.each(properties, function (key, value) {
                sensorsChanged(key, value.value); });
            return;
        }
    }

    function initStateGraph (svg) {
        svg.configure({width: '100%', height: '100%'}, false);
        // NB neither sensorList nor sensorTypes should actually change, so don't ask for updates
        torcconnection.subscribe('sensors', [], sensorsChanged, sensorSubscriptionChanged);

        //$("#dimmerin2", svg.root()).click(function () { $(this).attr("fill", "crimson"); });
    }

    function clearStateGraph () {
        // we need to explicitly destroy the svg
        $(".torc-central").svg("destroy");
        $(".torc-central").empty();
    }

    this.cleanup = function () {
        torcconnection = null;
        clearStateGraph();
        $(".torc-central").append("<div class='row text-center'><i class='fa fa-5x fa-exclamation-circle'></i>&nbsp;" + torc.SocketNotConnected + "</div>");
    };

    this.setup = function (connection) {
        torcconnection = connection;
        clearStateGraph();
        $(".torc-central").svg({loadURL: "../content/stategraph.svg", onLoad: initStateGraph});
    };
};


