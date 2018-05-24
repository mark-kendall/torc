/*jslint browser,devel,white */
/*global window,$,bootbox,torc,TorcConnection,TorcStateGraph,TorcAPI, template */

// declare theme to keep JSLint happy and aid syntax highlighter
var theme;

$(document).ready(function() {
    "use strict";

    var usermenu       = 'torc-user-menu';
    var secure         = false;
    var language       = undefined;
    var torcconnection = undefined;
    var torcstategraph = new TorcStateGraph($, torc);
    var torcapi        = new TorcAPI($, torc, usermenu);
    var torcsettings   = new TorcSettings($, torc, usermenu);

    function qsTranslate(context, string, disambiguation, plural, callback) {
        if (disambiguation === undefined) { disambiguation = ''; }
        if (plural === undefined) { plural = 0; }
        torcconnection.call('languages', 'GetTranslation',
                            { Context: context, String: string, Disambiguation: disambiguation, Number: plural},
                            function (translated) { if (typeof callback === 'function') { callback(translated); }},
                            function () { if (typeof callback === 'function') { callback(); }});
    }

    function addNavbarDropdown(dropdownClass, toggleClass, menuClass) {
        $('.navbar-nav').append(template(theme.NavbarDropdown, { "ddclass": dropdownClass, "icon": toggleClass, "menuclass": menuClass} ));
    }

    function removeNavbarDropdown(dropdownClass) {
        $('.' + dropdownClass).remove();
    }

    function addDropdownMenuDivider(menu, identifier) {
        $('.' + menu).append(template(theme.NavbarDropdownDivider, { "id": identifier }));
    }

    function addDropdownMenuItem(menu, identifier, link, text, click, hide) {
        $('.' + menu).append(template(theme.NavbarDropdownItem, {"id": identifier, "link": link, "text": text }));
        if (typeof click === 'function') { $('.' + identifier).on("click", click); }
    }

    function peerListChanged(name, value) {
        if (name === 'peers') {
            // remove old peers
            $(".torc-peer").remove();
            $(".torc-peer-status").remove();

            // and add new
            if ($.isArray(value) && value.length) {
                addDropdownMenuItem('torc-peer-menu', 'torc-peer-status', '#', '');
                qsTranslate('TorcNetworkedContext', '%n other Torc device(s) discovered', '', value.length,
                            function(result) { $(".torc-peer-status").html(result); });
                addDropdownMenuDivider('torc-peer-menu', 'torc-peer');
                value.forEach( function (element, index) {
                    addDropdownMenuItem('torc-peer-menu', 'torc-peer torc-peer' + index, 'http://' + element.address + ':' + element.port + '/index.html', '');
                    qsTranslate('TorcNetworkedContext', 'Connect to %1', '', 0,
                                function(result) { $(".torc-peer" + index).html(template(theme.DropdownItemWithIcon, { "icon": "external-link-square", "text": result.replace("%1", element.name) })); });
                });
            } else {
                addDropdownMenuItem('torc-peer-menu', 'torc-peer-status', '#', torc.NoPeers);
            }
        }
    }

    function peerSubscriptionChanged(version, ignore, properties) {
        if (version !== undefined && typeof properties === 'object' && properties.hasOwnProperty('peers') &&
            properties.peers.hasOwnProperty('value') && $.isArray(properties.peers.value)) {
            addNavbarDropdown('torc-peer-dropdown', 'link', 'torc-peer-menu');
            $.each(properties, function (key, value) {
                peerListChanged(key, value.value); });
            return;
        }

        // this is either a subscription failure or the socket was closed/disconnected
        removeNavbarDropdown('torc-peer-dropdown');
    }

    function languageChanged(name, value) {
        if (name === 'languageCode') {
            if (language === undefined) {
                console.log('Using language: ' + value);
                language = value;
            } else {
                if (language !== value) {
                    // simple, unambiguous strings are listed in the torc var. This needs to be
                    // reloaded if the language changes.
                    console.log("Language changed - reloading");
                    location.reload(true);
                }
            }
        }
    }

    function languageSubscriptionChanged(version, ignore, properties) {
        if (version !== undefined && typeof properties === 'object') {
            // this should happen when the socket status transitions to Connected but we don't have the
            // translation service at that point
            torcconnection.call('services', 'IsSecure', {},
                                function (result) {
                                    if (result.secure === true) {
                                        secure = true;
                                        $(".torc-socket-status-icon").removeClass("fa-check fa-exclamation-circle fa-question-circle-o fa-check-circle-o").addClass("fa-lock");
                                    }
                                    qsTranslate('TorcNetworkedContext', secure ? 'Connected securely to %1' : 'Connected to %1', '', 0,
                                                function (result) { $(".torc-socket-status-text").html(result.replace('%1', window.location.hostname)); });
                                });
            $.each(properties, function (key, value) {
                languageChanged(key, value.value); });
        }
    }

    function powerChanged(name, value) {
        var translatedName;
        var translatedConfirmation;
        var method;

        if (name === 'batteryLevel') {
            if (value === undefined) {
                translatedName = '';
            } else if (value === torc.ACPower) {
                translatedName = template(theme.DropdownItemWithIcon, { "icon": "bolt", "text": torc.ACPowerTr });
            } else if (value === torc.UnknownPower) {
                translatedName = torc.UnknownPowerTr;
            } else {
                translatedName = template(theme.DropdownItemWithIcon, { "icon": "tachometer", "text": value + '%' });
                qsTranslate('TorcPower', 'Battery %n%', '', value,
                            function (result) { $('.torc-power-status').html(template(theme.DropdownItemWithIcon, { "icon": "tachometer", "text": result })); });
            }

            $('.torc-power-status').html(translatedName);
            return;
        }

        if (name === 'canSuspend') {
            translatedName = template(theme.DropdownItemWithIcon, { "icon": "times", "text": torc.Suspend });
            translatedConfirmation = torc.ConfirmSuspend;
            method = 'Suspend';
        } else if (name === 'canShutdown') {
            translatedName = template(theme.DropdownItemWithIcon, { "icon": "times-circle", "text": torc.Shutdown });
            translatedConfirmation = torc.ConfirmShutdown;
            method = 'Shutdown';
        } else if (name === 'canHibernate') {
            translatedName = template(theme.DropdownItemWithIcon, { "icon": "moon-o", "text": torc.Hibernate });
            translatedConfirmation = torc.ConfirmHibernate;
            method = 'Hibernate';
        } else if (name === 'canRestart') {
            translatedName = template(theme.DropdownItemWithIcon, { "icon": "refresh", "text": torc.Restart });
            translatedConfirmation = torc.ConfirmRestart;
            method = 'Restart';
        } else { return; }

        if (value === false || value === undefined) {
            $('.torc-' + name).remove();
        } else {
            addDropdownMenuItem('torc-power-menu', 'torc-' + name, '#', translatedName,
                                function () {
                                    bootbox.confirm(translatedConfirmation, function(result) {
                                    if (result === true) { torcconnection.call('power', method); }
                                }); });
        }
    }

    function powerSubscriptionChanged(version, ignore, properties) {
        if (version !== undefined && typeof properties === 'object') {
            addNavbarDropdown('torc-power-dropdown', 'power-off', 'torc-power-menu');
            addDropdownMenuItem('torc-power-menu', 'torc-power-status', '#', '');
            addDropdownMenuDivider('torc-power-menu', '');
            $.each(properties, function (key, value) {
                powerChanged(key, value.value); });
            return;
        }

        removeNavbarDropdown('torc-power-dropdown');
    }

    function userChanged(name, value) {
        var translatedName;
        var translatedConfirmation;
        var method;

        if (name === 'canRestartTorc') {
            translatedName = template(theme.DropdownItemWithIcon, { "icon": "refresh", "text": torc.RestartTorcTr });
            translatedConfirmation = torc.ConfirmRestartTorc;
            method = 'RestartTorc';
        } else if (name === 'canStopTorc') {
            translatedName = template(theme.DropdownItemWithIcon, { "icon": "times", "text": torc.StopTorcTr });
            translatedConfirmation = torc.ConfirmStopTorc;
            method = 'StopTorc';
        } else { return; }

        if (value === false || value === undefined) {
            $('.torc-' + name).remove();
        } else {
            addDropdownMenuItem(usermenu, 'torc-' + name, '#', translatedName,
                                function () {
                                    bootbox.confirm(translatedConfirmation, function(result) {
                                    if (result === true) { torcconnection.call(torc.UserService, method); }
                                }); });
        }
    }

    function addFileModal(name, title, menu, contentSource, contentType) {
        var modalid     = name    + 'torcmodal';
        var contentid   = modalid + 'content';
        var menuid      = modalid + 'menu';
        $('.torc-navbar').after(template(theme.FileModal, { "id": modalid, "title": title, "contentid": contentid }));
        var item = template(theme.DropdownItemWithIcon, { "icon": "file-text-o", "text": menu });
        addDropdownMenuItem(usermenu, menuid, '#' + modalid, item,
                            contentSource !== '' ?
                                function () { $.ajax({ url: contentSource, dataType: contentType, xhrFields: { withCredentials: true }})
                                              .done(function (ignore, ignore2, xhr) { $('#' + contentid).text(xhr.responseText); }); } : '');
        $('.' + menuid + '').attr('data-toggle', 'modal');
    }

    function addComplexModal(name, title, menu, setup, shown, hidden) {
        addFileModal(name, title, menu);
        var modalid = name + 'torcmodal';
        if (typeof setup  === 'function') { setup(); };
        if (typeof shown  === 'function') { $('#' + modalid).on('show.bs.modal', shown); };
        if (typeof hidden === 'function') { $('#' + modalid).on('hidden.bs.modal', hidden); };
    }

    function addLogModal(name, title, menu) {
        var modalid   = name + 'torcmodal';
        var contentid = modalid + 'content';
        function load () {
            $.ajax({ url: '/services/log/GetLog', dataType: 'text', xhrFields: { withCredentials: true }})
                .done(function (ignore, ignore2, xhr) { $('#' + contentid).text(xhr.responseText); });
        };
        addComplexModal(name, title, menu,
            function() { // setup
                $('#' + modalid + ' h4').replaceWith('<button type="button" class="btn btn-info invisible torclogrefresh">' + torc.RefreshTr + '</button>');
                $('#' + modalid + ' .modal-footer').prepend('<button type="button" class="btn btn-info invisible torclogrefresh">' + torc.RefreshTr + '</button>');
                $('.torclogrefresh').on("click",
                    function () {
                        load();
                        $('.torclogrefresh').addClass('invisible');
                    });
            },
            function () { // show
                torcconnection.subscribe('log', ['log'],
                    function (name, value) {
                        if (name === 'log') { $('.torclogrefresh').removeClass('invisible'); }
                    });
                load();
            },
            function () { // invisible
                $('.torclogrefresh').addClass('invisible');
                torcconnection.unsubscribe('log');
            });
    }

    function addLogtailModal(name, title, menu) {
        var modalid   = name + 'torcmodal';
        var contentid = modalid + 'content';
        var following = false;
        addComplexModal(name, title, menu,
            function () { // setup
                $('#' + modalid + ' h4').replaceWith('<button type="button" class="btn btn-info invisible torclogtoggle">' + torc.FollowTr + '</button>');
                $('#' + modalid + ' .modal-footer').prepend('<button type="button" class="btn btn-info invisible torclogtoggle">' + torc.FollowTr + '</button>');
                $('.torclogtoggle').on("click", function() {
                    if (following) {
                        following = false;
                        $('.torclogtoggle').text(torc.FollowTr);
                    } else {
                        following = true;
                        $('.torclogtoggle').text(torc.UnfollowTr);
                        $('#' + modalid).animate( { scrollTop: ($('#' + modalid)[0].scrollHeight - $('#' + modalid).height()) }, 250);
                    }
                });
            },
            function () { // show
                $('#' + contentid).text('');
                torcconnection.subscribe('log', ['tail'],
                    function (name, value) {
                        if (name === 'tail') {
                            var current = $('#' + contentid).text();
                            $('#' + contentid).text(current + value);
                            if ($('#' + modalid)[0].scrollHeight > $('#' + modalid).height()) { $('.torclogtoggle').removeClass('invisible'); };
                            if (following === true) { $('#' + modalid).scrollTop($('#' + modalid)[0].scrollHeight - $('#' + modalid).height()); };
                        };
                    },
                    function (version, ignore, properties) {
                        $.each(properties, function (key, value) {
                            if (key === 'tail') {
                                $('#' + contentid).text(value.value);
                            }
                        });
                    });
            },
            function () { // invisible
                following = false;
                $('.torclogtoggle').addClass('invisible').text(torc.FollowTr);
                torcconnection.unsubscribe('log');
            });
    }

    function userSubscriptionChanged(version, ignore, properties) {
        if (version !== undefined && typeof properties === 'object') {
            addNavbarDropdown('torc-user-dropdown', torc.UserService, usermenu);
            // we need the user menu before adding other options
            // NB userSubscriptionChanged should only ever be called once...
            addDropdownMenuItem(usermenu, usermenu + '-user', '#', torc.LoggedInUserTr.replace("%1", properties.userName.value));
            addDropdownMenuDivider(usermenu, '');
            addFileModal('config',  torc.ViewConfigTitleTr, torc.ViewConfigTr, torc.TorcConfFile,         'xml');
            addFileModal('xsd',     torc.ViewXSDTitleTr,    torc.ViewXSDTr,    '/content/torc.xsd',       'xml');
            addFileModal('dot',     torc.ViewDOTTitleTr,    torc.ViewDOTTr,    '/content/stategraph.dot', 'text');
            addLogModal('log', '',  torc.ViewLogTr);
            addLogtailModal('logtail', '', torc.FollowLogTr);
            torcapi.setup(torcconnection);
            torcsettings.setup(torcconnection);
            addDropdownMenuDivider(usermenu, '');
            $.each(properties, function (key, value) {
                userChanged(key, value.value); });
            return;
        }

        removeNavbarDropdown('torc-user-dropdown');
    }

    function timeChanged(name, value) {
        if (name === 'currentTime') {
            $('.torc-time-value').html(value);
        }
    }

    function timeSubscriptionChanged(version, ignore, properties) {
        if (version !== undefined && typeof properties === 'object') {
            addNavbarDropdown('torc-time-dropdown', 'clock-o', 'torc-time-menu');
            addDropdownMenuItem('torc-time-menu', 'torc-time-value', '#', 'dummy');
            $.each(properties, function (key, value) {
                timeChanged(key, value.value); });
            return;
        }

        removeNavbarDropdown('torc-time-dropdown');
    }


    function statusChanged (status) {
        if (status === torc.SocketNotConnected) {
            $(".torc-socket-status-icon").removeClass("fa-check fa-check-circle-o-circle-o fa-question-circle fa-lock").addClass("fa-exclamation-circle");
            $(".torc-socket-status-text").html(torc.SocketNotConnected);
            // remove modals
            $('.torcmodal').remove();
            $('.modal-backdrop').remove();
            // remove state graph
            torcstategraph.cleanup();
            torcapi.cleanup();
            torcsettings.cleanup();
        } else if (status === torc.SocketConnecting) {
            $(".torc-socket-status-icon").removeClass("fa-check fa-check-circle-o fa-exclamation-circle").addClass("fa-question-circle");
            $(".torc-socket-status-text").html(torc.SocketConnecting);
        } else if (status === torc.SocketConnected) {
            $(".torc-socket-status-icon").removeClass("fa-exclamation-circle fa-check-circle-o fa-question-circle").addClass("fa-check");
        } else if (status === torc.SocketReady) {
            $(".torc-socket-status-icon").removeClass("fa-check fa-exclamation-circle fa-question-circle").addClass("fa-check-circle-o");
            torcconnection.subscribe('languages', ['languageString', 'languageCode', 'languages'], languageChanged, languageSubscriptionChanged);
            torcconnection.subscribe('peers', ['peers'], peerListChanged, peerSubscriptionChanged);
            torcconnection.subscribe('power', ['canShutdown', 'canSuspend', 'canRestart', 'canHibernate', 'batteryLevel'], powerChanged, powerSubscriptionChanged);
            torcconnection.subscribe(torc.UserService, ['canRestartTorc'], userChanged, userSubscriptionChanged);
            torcconnection.subscribe('time', ['currentTime'], timeChanged, timeSubscriptionChanged);
            torcstategraph.setup(torcconnection);
        }
    }

    // add the nav bar
    $('body').prepend(template(theme.Navbar, {"torc": torc.ServerApplication }));

    // add a socket status/connection dropdown with icon
    addNavbarDropdown('', 'check torc-socket-status-icon', 'torc-socket-menu');

    // add the connection text item
    addDropdownMenuItem('torc-socket-menu', 'torc-socket-status-text', '#', '');

    // connect
    statusChanged(torc.SocketNotConnected);
    torcconnection = new TorcConnection($, torc, statusChanged);
});
