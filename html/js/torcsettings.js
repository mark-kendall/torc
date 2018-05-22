/* TorcSettings.js
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2018
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

var TorcSettings = function ($, torc, menu) {
    "use strict";

    var torcconnection  = undefined;

    function load () {
        // add credentials first
        if (typeof torcconnection !== 'object') { return; }

        var servicelist = torcconnection.getServiceList();
        if (servicelist.hasOwnProperty(torc.UserService)) {
            $('.' + theme.SettingsModalContentID).html(theme.SettingsCredentialsButton);
            $('#' + theme.SettingsCredentialsButtonId).on('click', function () {
                function togglevalid (a,b) {
                    if (regex.test(a.val()) && a.val() === b.val()) {
                        a.removeClass("is-invalid").addClass("is-valid");
                        b.removeClass("is-invalid").addClass("is-valid");
                    } else {
                        a.removeClass("is-valid").addClass("is-invalid");
                        b.removeClass("is-valid").addClass("is-invalid");
                    }
                }

                var regex = /^\w{4,}$/;
                var creds = bootbox.dialog({
                    className: 'torcmodal',
                    title: ' ',//torc.ChangeCredsTr,
                    message: theme.SettingsCredentialsForm,
                    buttons: {
                        cancel: {
                            label: torc.CancelTr
                        },
                        ok: {
                            label: torc.ConfirmTr,
                            className: 'btn-danger',
                            callback: function (result) {
                                var user1 = $('#' + theme.SettingsUsername1).val();
                                var user2 = $('#' + theme.SettingsUsername2).val();
                                var pwd1  = $('#' + theme.SettingsPassword1).val();
                                var pwd2  = $('#' + theme.SettingsPassword2).val();
                                if (!(user1 === user2 && pwd1 === pwd2 && regex.test(user1) && regex.test(pwd1))) {
                                    // highlight errored fields
                                    togglevalid($('#' + theme.SettingsUsername1), $('#' + theme.SettingsUsername2));
                                    togglevalid($('#' + theme.SettingsPassword1), $('#' + theme.SettingsPassword2));
                                    return false; }
                                torcconnection.call(torc.UserService, 'SetUserCredentials',
                                                    { 'Name' : user1, 'Credentials': md5(user1 + ':' + torc.TorcRealm + ':' + pwd1) });
                            }
                        }
                    }
                });

                creds.init( function () {
                    $('#' + theme.SettingsUsername1).on('input', function () { togglevalid($(this), $('#' + theme.SettingsUsername2)); });
                    $('#' + theme.SettingsUsername2).on('input', function () { togglevalid($(this), $('#' + theme.SettingsUsername1)); });
                    $('#' + theme.SettingsPassword1).on('input', function () { togglevalid($(this), $('#' + theme.SettingsPassword2)); });
                    $('#' + theme.SettingsPassword2).on('input', function () { togglevalid($(this), $('#' + theme.SettingsPassword1)); });
                });
            });
          }
    }

    function clearSettings () {
        // remove the modal
        $('#' + theme.SettingsModalID).remove();
    }

    this.cleanup = function () {
        torcconnection = undefined;
        clearSettings();
    };

    this.setup = function (connection) {
        torcconnection = connection;

        // add the modal
        $('.torc-navbar').after(template(theme.SettingsModal, { "title": torc.SettingsTr }));

        // and the menu item to display the modal
        var item = template(theme.DropdownItemWithIcon, { "icon": "cog", "text": torc.SettingsTr });
        $('.' + menu).append(template(theme.NavbarDropdownItem, {"id": theme.SettingsModalID + "-menu", "link": "#" + theme.SettingsModalID, "text": item }));
        $('.' + theme.SettingsModalID + '-menu').attr('data-toggle', 'modal');

        load();
    };
};
