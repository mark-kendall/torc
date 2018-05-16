/* TorcAPI.js
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

// declare vars to aid syntax highlighter and keep JSLint happy
var theme;
var template;

var TorcAPI = function ($, torc) {
    "use strict";

    var torcconnection  = undefined;
    var servicelist     = undefined;
    var returnformats   = undefined;
    var websocketprotos = undefined;

    function setServiceDetail(Service, Methods, Properties) {
        $('#' + theme.APIServiceDetail + Service).html(template(theme.APIServiceMethods, { "methods": Methods, "properties": Properties }));
    }

    function load (Service) {
        if (typeof torcconnection === 'object') {
            if (servicelist === undefined) { servicelist = torcconnection.getServiceList(); };
            if (returnformats === undefined) { returnformats = torcconnection.getReturnFormats(); };
            if (websocketprotos === undefined) { websocketprotos = torcconnection.getWebSocketProtocols(); };
        }

        // these aren't going to change
        if (typeof servicelist === 'object' && typeof returnformats === 'object' && typeof websocketprotos === 'object') {
            // build the API page
            $('.' + theme.APIModalContentID).html(template(theme.APIServiceList, { "services": servicelist, "formats": returnformats, "subprotocols": websocketprotos }));
            // iterate over services
            Object.getOwnPropertyNames(servicelist).forEach( function(service) {
                // make service collapse buttons dynamic (up/down)
                var button = $('button[data-target="#api-detail-' + service + '"]');
                $('#api-detail-' + service).on('hide.bs.collapse', function() {
                    button.html(template(theme.APICollapseShow));
                    $('#api-detail2-' + service).removeClass('bg-secondary');
                });
                $('#api-detail-' + service).on('show.bs.collapse', function() {
                    $('#api-detail2-' + service).addClass('bg-secondary');
                    button.html(template(theme.APICollapseHide));
                    // scroll to top of selected service
                    // TODO make this animated - the scrollTop(0) is a hack that breaks animation
                    $('#torc-api-modal').scrollTop(0);
                    $('#torc-api-modal').scrollTop($('#api-detail2-' + service).offset().top);
                });

                // load method/property details
                var methods = torcconnection.getServiceMethods(service);
                var properties = torcconnection.getServiceProperties(service);
                // we are subscribed and details are available
                if (methods !== undefined && properties !== undefined) {
                    setServiceDetail(service, methods, properties);
                } else {
                // need to ask for details
                    torcconnection.call('services', 'GetServiceDescription', { "Service": service }, function (result) {
                        setServiceDetail(service, result.methods, result.properties); });
                }
            });
        }
    }

    function clearAPI () {
        // remove the modal
        $('#' + theme.APIModalID).remove();
    }

    this.cleanup = function () {
        torcconnection = undefined;
        clearAPI();
    };

    this.setup = function (connection) {
        torcconnection = connection;
        clearAPI();

        // add the modal
        $('.torc-navbar').after(template(theme.APIModal, { "title": torc.ViewAPITitleTr }));

        // and the menu item to display the modal
        var item = template(theme.DropdownItemWithIcon, { "icon": "github", "text": torc.ViewAPITr });
        $('.torc-central-menu').append(template(theme.NavbarDropdownItem, {"id": theme.APIModalID + "-menu", "link": "#" + theme.APIModalID, "text": item }));
        $('.' + theme.APIModalID + '-menu').attr('data-toggle', 'modal');

        load();
    };
};
