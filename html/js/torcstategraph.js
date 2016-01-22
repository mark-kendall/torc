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
/*global $*/

/* TODO
  use unique string identifiers rather than Value, Valid etc
  translated strings for value, valid etd
*/

var TorcStateGraph = function ($, torc) {
    "use strict";

    var torcconnection = undefined;
    var sensorTypes    = undefined;
    var controlTypes   = undefined;
    var outputTypes    = undefined;
    var stategraph     = undefined;

    function deviceChanged (name, value, service) {
        if (name === undefined  || value === undefined || service === undefined) {
            return;
        }

        if (name === 'value') {
            $('#' + service + '_value').text('Value: ' + value.toFixed(2));
        } else if (name === 'valid') {
            $('#' + service + '_valid').text('Valid: ' + value);
            $('#' + service + '_background').attr('fill', value ? 'none' : 'crimson');
        }
    }

    function deviceSubscriptionChanged (version, ignore, properties, service) {
        if (version !== undefined && typeof properties === 'object') {
            $.each(properties, function (key, value) {
                deviceChanged(key, value.value, service);
            });
            return;
        }
    }

    function setupDevice (name) {
        // add id's to certain svg elements for each device
        var root = stategraph.root();
        $('#' + name + ' > text:contains("Value")', root).attr('id', name + '_value');
        $('#' + name + ' > text:contains("Valid")', root).attr('id', name + '_valid');
        $('#' + name + ' > polygon',                root).attr('id', name + '_background');
        torcconnection.subscribe(name, ['value', 'valid'], deviceChanged, deviceSubscriptionChanged);
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
                                setupDevice(sensor);
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

    function outputsChanged (name, value) {
        if (name === 'outputTypes' && $.isArray(value)) {
            outputTypes = value;
        } else if (name === 'outputList' && typeof value === 'object') {
            if ($.isArray(outputTypes) && outputTypes.length > 0) {
                $.each(outputTypes, function (ignore, type) {
                    $.each(value, function (key, outputs) {
                        if (key === type && $.isArray(outputs)) {
                            $.each(outputs, function (ignore, output) {
                                setupDevice(output);
                            });
                        }
                    });
                });
            }
        }
    }

    function outputSubscriptionChanged(version, ignore, properties) {
        if (version !== undefined && typeof properties === 'object') {
            // NB update outputTypes first so that we can iterate over outputList meaningfully
            outputsChanged('outputTypes', properties.outputTypes.value);
            $.each(properties, function (key, value) {
                outputsChanged(key, value.value); });
            return;
        }
    }

    function controlsChanged (name, value) {
        if (name === 'controlTypes' && $.isArray(value)) {
            controlTypes = value;
        } else if (name === 'controlList' && typeof value === 'object') {
            if ($.isArray(controlTypes) && controlTypes.length > 0) {
                $.each(controlTypes, function (ignore, type) {
                    $.each(value, function (key, controls) {
                        if (key === type && $.isArray(controls)) {
                            $.each(controls, function (ignore, control) {
                                setupDevice(control);
                            });
                        }
                    });
                });
            }
        }
    }

    function controlSubscriptionChanged(version, ignore, properties) {
        if (version !== undefined && typeof properties === 'object') {
            // NB update controlTypes first so that we can iterate over controlList meaningfully
            controlsChanged('controlTypes', properties.controlTypes.value);
            $.each(properties, function (key, value) {
                controlsChanged(key, value.value); });
            return;
        }
    }

    function initStateGraph (svg) {
        stategraph = svg;
        stategraph.configure({width: '100%', height: '100%'}, false);
        // NB neither sensorList nor sensorTypes should actually change, so don't ask for updates
        torcconnection.subscribe('sensors',  [], sensorsChanged,  sensorSubscriptionChanged);
        torcconnection.subscribe('controls', [], controlsChanged, controlSubscriptionChanged);
        torcconnection.subscribe('outputs',  [], outputsChanged,  outputSubscriptionChanged);
    }

    function clearStateGraph () {
        // we need to explicitly destroy the svg
        $("#torc-central").svg("destroy");
        $("#torc-central").empty();
        stategraph = undefined;
    }

    this.cleanup = function () {
        torcconnection = undefined;
        clearStateGraph();
        $("#torc-central").append("<div class='row text-center'><i class='fa fa-5x fa-exclamation-circle'></i>&nbsp;" + torc.SocketNotConnected + "</div>");
    };

    this.setup = function (connection) {
        torcconnection = connection;
        clearStateGraph();
        $("#torc-central").svg({loadURL: "../content/stategraph.svg", onLoad: initStateGraph});
    };
};

// add the central element
$(document).ready(function() {
    "use strict";
    $('.navbar-fixed-top').after($('<div/>').attr('id', 'torc-central'));
});



