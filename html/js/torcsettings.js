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
    var subscriptions = [];

    function load () {
        // credentials need special handling
        if (typeof torcconnection !== 'object') { return; }

        var servicelist = torcconnection.getServiceList();
        if (servicelist.hasOwnProperty(torc.UserService)) {
            $('.' + theme.SettingsModalContentID).html(theme.SettingsCredentialsButton);
            $('#' + theme.SettingsCredentialsButtonId).on('click', function () { $('#' + theme.SettingsCredentialsFormID).collapse('toggle'); });
            $('#' + theme.SettingsCredentialsFormID).on('show.bs.collapse', function() { $('#' + theme.SettingsCredentialsButtonId).text(torc.CloseTr); });
            $('#' + theme.SettingsCredentialsFormID).on('hide.bs.collapse', function() {
                $('#' + theme.SettingsCredentialsButtonId).text(torc.UpdateTr);
                $('#' + theme.SettingsUsername1).val('').removeClass('is-invalid');
                $('#' + theme.SettingsUsername2).val('').removeClass('is-invalid');
                $('#' + theme.SettingsPassword1).val('').removeClass('is-invalid');
                $('#' + theme.SettingsPassword2).val('').removeClass('is-invalid');
            });
            var regex = /^\w{4,}$/;
            function togglevalid (a,b) {
                if (regex.test(a.val()) && a.val() === b.val()) {
                    a.removeClass("is-invalid").addClass("is-valid");
                    b.removeClass("is-invalid").addClass("is-valid");
                } else {
                    a.removeClass("is-valid").addClass("is-invalid");
                    b.removeClass("is-valid").addClass("is-invalid");
                }
            }
            $('#' + theme.SettingsUsername1).on('input', function () { togglevalid($(this), $('#' + theme.SettingsUsername2)); });
            $('#' + theme.SettingsUsername2).on('input', function () { togglevalid($(this), $('#' + theme.SettingsUsername1)); });
            $('#' + theme.SettingsPassword1).on('input', function () { togglevalid($(this), $('#' + theme.SettingsPassword2)); });
            $('#' + theme.SettingsPassword2).on('input', function () { togglevalid($(this), $('#' + theme.SettingsPassword1)); });

            $('#' + theme.SettingsCredentialsConfirmID).on('click', function () {
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
                });
            }

        // settings
        function showsettings (result) {
            $('.' + theme.SettingsModalContentID).append(template(theme.SettingsList, { "Settings": result}));

            // iterate over the results
            function setupRecursive (setting) {
                if (setting.hasOwnProperty('uiname') && setting.hasOwnProperty('name') && setting.hasOwnProperty('type')) {
                    if (setting.hasOwnProperty('children')) { Object.getOwnPropertyNames(setting.children).forEach(function (child) { setupRecursive(setting.children[child]); })}
                    var name = setting.name;
                    var id   = 'torc-settings-' + name;
                    var but  = 'torc-settings-button-' + name;
                    var col  = 'torc-settings-button-col-' + name;
                    var type = setting.type;
                    var serverValue, min, max, selections;
                    function getValue () {
                        if (type === 'bool')    { return $('#' + id).is(':checked'); }
                        if (type === 'string')  { return $('#' + id).val(); }
                        if (type === 'integer') { return Number($('#' + id).val()); }
                    }
                    function setValid (Valid) {
                        if (Valid) { $('#' + id).removeClass('is-invalid'); } else { $('#' + id).addClass('is-invalid'); }
                    }
                    function toggleButton () {
                        var value = getValue();
                        var valid = true;
                        if (type === 'integer' && (value < min || value > max)) {
                            setValid(false);
                            valid = false;
                        } else {
                            setValid(true);
                        }
                        if (value === serverValue || !valid) { $('#' + col).collapse('hide'); } else { $('#' + col).collapse('show'); }
                    }
                    $('#' + but).on('click', function () {
                        var value = getValue();
                        if (value === serverValue) { return; }
                        torcconnection.call(name, 'SetValue', { Value: value },
                            function (success) {
                                if (success === true) {
                                    setValue(value); toggleButton();
                                    if (name === torc.PortSetting) {
                                        window.location.replace(window.location.protocol + '//' + window.location.hostname + ':' + value);
                                    } else if (name === torc.SecureSetting) {
                                        var port = window.location.port;
                                        if (!port || 0 === port.length) { if (window.location.protocol === 'https:') { port = 443; } else { port = 80; }}
                                        window.location.replace((value ? 'https:' : 'http:') + '//' + window.location.hostname + ':' + port);
                                    }
                                } else { setValid(false); }}
                        );
                    });
                    function setupEvents () {
                        var event = (type === 'bool' || selections !== undefined) ? 'change' : 'input';
                        $('#' + id).on(event, function () { toggleButton(); });
                    }
                    function setActive (value) {
                        $('#' + id).prop('disabled', !value);
                    }
                    function setValue (value) {
                        serverValue = value;
                        if (type === 'bool')    { $('#' + id).prop('checked', value); }
                        if (type === 'string')  { $('#' + id).val(value); }
                        if (type === 'integer') { $('#' + id).val(value); }
                    }
                    function update (Name, Value) {
                        if (Name === 'value')    { setValue(Value); }
                        if (Name === 'isActive') { setActive(Value); }
                    }
                    torcconnection.subscribe(name, ['value', 'isActive'],
                        function (Name, Value) {
                            update(Name, Value);
                        },
                        function (version, ignore, properties) {
                            if (version === undefined || typeof properties !== 'object') { return; }
                            subscriptions.push(name);
                            if (properties.hasOwnProperty('isActive')) { update('isActive', properties.isActive.value); }
                            if (properties.hasOwnProperty('value'))    { update('value',    properties.value.value); }
                            if (properties.hasOwnProperty('helpText')) { $('#' + id).after(template(theme.SettingsListHelpText, { "Help": properties.helpText.value})); }
                            if (type === 'integer') {
                                torcconnection.call(name, 'GetBegin', [], function (begin) { $('#' + id).prop('min',  begin); min = begin; });
                                torcconnection.call(name, 'GetEnd',   [], function (end)   { $('#' + id).prop('max',  end);   max = end;   });
                                torcconnection.call(name, 'GetStep',  [], function (step)  { $('#' + id).prop('step', step); });
                            }
                            if (properties.hasOwnProperty('selections') && !$.isEmptyObject(properties.selections.value)) {
                                selections = properties.selections.value;
                                if (Object.keys(selections).length > 1) {
                                    $('#' + id).replaceWith(template(theme.SettingsListSelect, { "ID": id, "Selections": selections}));
                                    $('#' + id).val(serverValue);
                                } else {
                                    update('isActive', false);
                                }
                            }
                            setupEvents();
                        });
                }
            }
            setupRecursive(result);
        }

        if (servicelist.hasOwnProperty(torc.RootSetting)) { torcconnection.call(torc.RootSetting, 'GetChildList', {}, function (result) { showsettings(result); }); }
    }

    function unload() {
        for (var i = 0; i < subscriptions.length; i++) { torcconnection.unsubscribe(subscriptions[i]); }
        while (subscriptions.length) { subscriptions.pop(); }
        $('.' + theme.SettingsModalContentID).empty();
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
        $('#' + theme.SettingsModalID).on('show.bs.modal', function () { load(); });
        $('#' + theme.SettingsModalID).on('hidden.bs.modal', function () { unload(); });
    };
};
