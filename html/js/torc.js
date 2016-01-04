/*jslint browser,devel,white */
/*global window,$,bootbox,torc,TorcConnection,TorcStateGraph */

$(document).ready(function() {
    "use strict";

    var language       = undefined;
    var torcconnection = undefined;
    var torcstategraph = new TorcStateGraph($, torc);

    function qsTranslate(context, string, disambiguation, plural, callback) {
        if (disambiguation === undefined) { disambiguation = ''; }
        if (plural === undefined) { plural = 0; }
        torcconnection.call('languages', 'GetTranslation',
                            { Context: context, String: string, Disambiguation: disambiguation, Number: plural},
                            function (translated) { if (typeof callback === 'function') { callback(translated); }},
                            function () { if (typeof callback === 'function') { callback(); }});
    }

    function addNavbarDropdown(dropdownClass, toggleClass, listClass) {
        $('<li/>', { class: 'dropdown ' + dropdownClass})
          .append($('<a/>', {
            class: 'dropdown-toggle',
            href:  '#'})
            .attr('data-toggle', 'dropdown')
                  .append($('<i/>', { class: 'fa ' + toggleClass + ' fa-lg'})))
          .append($('<ul/>', {
            class: 'dropdown-menu ' + listClass,
            role: 'menu'
          }))
          .appendTo('.navbar-nav');
    }

    function removeNavbarDropdown(dropdownClass) {
        $('.' + dropdownClass).remove();
    }

    function addDropdownMenuDivider(menu, identifier) {
        $('.' + menu).append($('<li/>', { class: 'divider ' + identifier} ));
    }

    function addDropdownMenuItem(menu, identifier, link, text, click) {
        var item = $('<li/>', { class: identifier }).append($('<a/>', { href: link, html: text } ));
        if (typeof click === 'function') { item.click(click); }
        item.appendTo($('.' + menu));
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
                            function(result) { $(".torc-peer-status a").html(result); });
                addDropdownMenuDivider('torc-peer-menu', 'torc-peer');
                value.forEach( function (element, index) {
                    addDropdownMenuItem('torc-peer-menu', 'torc-peer torc-peer' + index, 'http://' + element.address + ':' + element.port + '/html/index.html', '');
                    qsTranslate('TorcNetworkedContext', 'Connect to %1', '', 0,
                                function(result) { $(".torc-peer" + index + " a").html('<i class=\'fa fa-external-link-square\'>&nbsp</i>' + result.replace("%1", element.name)); });
                });
            } else {
                addDropdownMenuItem('torc-peer-menu', 'torc-peer-status', '#', torc.NoPeers);
            }
        }
    }

    function peerSubscriptionChanged(version, ignore, properties) {
        if (version !== undefined && typeof properties === 'object' && properties.hasOwnProperty('peers') &&
            properties.peers.hasOwnProperty('value') && $.isArray(properties.peers.value)) {
            addNavbarDropdown('torc-peer-dropdown', 'fa-link', 'torc-peer-menu');
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
                    location.reload();
                }
            }
        }
    }

    function languageSubscriptionChanged(version, ignore, properties) {
        if (version !== undefined && typeof properties === 'object') {
            // this should happen when the socket status transitions to Connected but we don't have the
            // translation service at that point
            qsTranslate('TorcNetworkedContext', 'Connected to %1', '', 0,
                        function (result) { $(".torc-socket-status-text a").html(result.replace('%1', window.location.host)); });
            $.each(properties, function (key, value) {
                languageChanged(key, value.value); });
        }
    }

    function powerChanged(name, value) {
        var translatedName, translatedConfirmation, method;

        if (name === 'batteryLevel') {
            if (value === undefined) {
                translatedName = '';
            } else if (value === torc.ACPower) {
                translatedName = '<i class=\'fa fa-bolt\'>&nbsp</i>' + torc.ACPowerTr;
            } else if (value === torc.UnknownPower) {
                translatedName = torc.UnknownPowerTr;
            } else {
                translatedName = '<i class=\'fa fa-tachometer\'>&nbsp</i>' + value + '%';
                qsTranslate('TorcPower', 'Battery %n%', '', value,
                            function (result) { $('.torc-power-status a').html('<i class=\'fa fa-tachometer\'>&nbsp</i>' + result); });
            }

            $('.torc-power-status a').html(translatedName);
            return;
        }

        if (name === 'canSuspend') {
            translatedName = '<i class=\'fa fa-times\'>&nbsp</i>' + torc.Suspend;
            translatedConfirmation = torc.ConfirmSuspend;
            method = 'Suspend';
        } else if (name === 'canShutdown') {
            translatedName = '<i class=\'fa fa-times-circle\'>&nbsp</i>' + torc.Shutdown;
            translatedConfirmation = torc.ConfirmShutdown;
            method = 'Shutdown';
        } else if (name === 'canHibernate') {
            translatedName = '<i class=\'fa fa-moon-o\'>&nbsp</i>' + torc.Hibernate;
            translatedConfirmation = torc.ConfirmHibernate;
            method = 'Hibernate';
        } else if (name === 'canRestart') {
            translatedName = '<i class=\'fa fa-refresh\'>&nbsp</i>' + torc.Restart;
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
            addNavbarDropdown('torc-power-dropdown', 'fa-power-off', 'torc-power-menu');
            addDropdownMenuItem('torc-power-menu', 'torc-power-status', '#', '');
            addDropdownMenuDivider('torc-power-menu', '');
            $.each(properties, function (key, value) {
                powerChanged(key, value.value); });
            return;
        }

        removeNavbarDropdown('torc-power-dropdown');
    }

    function centralChanged(name, value) {
        var translatedName, translatedConfirmation, method;

        if (name === 'canRestartTorc') {
            translatedName = '<i class=\'fa fa-refresh\'>&nbsp</i>' + torc.RestartTorcTr;
            translatedConfirmation = torc.ConfirmRestartTorc;
            method = 'RestartTorc';
        } else { return; }

        if (value === false || value === undefined) {
            $('.torc-' + name).remove();
        } else {
            addDropdownMenuItem('torc-central-menu', 'torc-' + name, '#', translatedName,
                                function () {
                                    bootbox.confirm(translatedConfirmation, function(result) {
                                    if (result === true) { torcconnection.call('central', method); }
                                }); });
        }
    }

    function centralSubscriptionChanged(version, ignore, properties) {
        if (version !== undefined && typeof properties === 'object') {
            addNavbarDropdown('torc-central-dropdown', 'fa-cog', 'torc-central-menu');
            $.each(properties, function (key, value) {
                centralChanged(key, value.value); });
            return;
        }

        removeNavbarDropdown('torc-central-dropdown');
    }

    function statusChanged (status) {
        if (status === torc.SocketNotConnected) {
            $(".torc-socket-status-icon").removeClass("fa-check fa-check-circle-o-circle-o fa-question-circle").addClass("fa-exclamation-circle");
            $(".torc-socket-status-text a").html(torc.SocketNotConnected);

            torcstategraph.cleanup();
        } else if (status === torc.SocketConnecting) {
            $(".torc-socket-status-icon").removeClass("fa-check fa-check-circle-o fa-exclamation-circle").addClass("fa-question-circle");
            $(".torc-socket-status-text a").html(torc.SocketConnecting);
        } else if (status === torc.SocketConnected) {
            $(".torc-socket-status-icon").removeClass("fa-exclamation-circle fa-check-circle-o fa-question-circle").addClass("fa-check");
        } else if (status === torc.SocketReady) {
            $(".torc-socket-status-icon").removeClass("fa-check fa-exclamation-circle fa-question-circle").addClass("fa-check-circle-o");
            torcconnection.subscribe('languages', ['languageString', 'languageCode', 'languages'], languageChanged, languageSubscriptionChanged);
            torcconnection.subscribe('peers', ['peers'], peerListChanged, peerSubscriptionChanged);
            torcconnection.subscribe('power', ['canShutdown', 'canSuspend', 'canRestart', 'canHibernate', 'batteryLevel'], powerChanged, powerSubscriptionChanged);
            torcconnection.subscribe('central', ['canRestartTorc'], centralChanged, centralSubscriptionChanged);

            torcstategraph.setup(torcconnection);
        }
    }

    // set 'brand'
    $(".navbar-brand").html(torc.ServerApplication);

    // add a socket status/connection dropdown with icon
    addNavbarDropdown('', 'torc-socket-status-icon', 'torc-socket-menu');

    // add the connection text item
    addDropdownMenuItem('torc-socket-menu', 'torc-socket-status-text', '#', '');

    // connect
    statusChanged(torc.SocketNotConnected);
    torcconnection = new TorcConnection($, torc, statusChanged);
});
