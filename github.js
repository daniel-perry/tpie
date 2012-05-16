if (!window.console || !console.log)
    console = {log: function () {}}; // all your logs are belong to /dev/null

// JSON-P callback
// https://api.github.com/repos/thomasmoelhave/tpie/events?callback=tpieevents
function tpieevents(input) {
    parse_events(input);
    store_remote_events(input);
}

function parse_events(input) {
    var data = input.data;
    var htmlitems = [];
    for (var i = 0, l = data.length; i < l; ++i) {
        handleevent(data[i], htmlitems);
        if (htmlitems.length >= 5) break;
    }
    var eventhtml = htmlitems.length ? htmlitems.join('\n') : '';
    var activity = document.getElementById('githubactivity');
    activity.innerHTML = eventhtml;
}

function handleevent(ev, html) {
    switch (ev.type) {
        case "PushEvent":
            html.push(handleanyevent(ev, 'pushed to '+printref(ev.payload.ref)));
            for (var i = 0, l = ev.payload.commits.length; i < l; ++i) {
                if (i > 2) {
                    html.push("and "+(l-i)+" more commits");
                    break;
                }
                var commit = ev.payload.commits[i];
                html.push(['<li class="commit"><a href="https://github.com/thomasmoelhave/tpie/commit/',commit.sha,'">',
                          commit.sha.substring(0,7),'<\/a> ',
                          commit.message.substring(0,70).replace(/&/g,'&amp;').replace(/</g,'&lt;'),
                          '</li>'].join(''));
            }
            break;
        case "CreateEvent":
            var name = ev.payload.ref;
            if (ev.payload.ref_type == 'branch') name = branchlink(name);
            html.push(handleanyevent(ev, 'created '+ev.payload.ref_type+' '+name));
            break;
    }
}

// ISO 8601 specifies YYYY-MM-DDTHH:MM:SSZ
function printdate(iso8601) {
    var d = new Date(iso8601);
    var now = new Date;
    if (d.getFullYear() != now.getFullYear() || d.getMonth() != now.getMonth() || d.getDate() != now.getDate()) {
        return 'on '+d.toLocaleDateString();
    }
    return 'on '+d.toLocaleTimeString();
}

function branchlink(branch) {
    return '<a href="https://github.com/thomasmoelhave/tpie/tree/'+branch+'">'+branch+'</a>';
}

function printref(refname) {
    if (refname.substring(0, 11) == 'refs/heads/') {
        var branch = refname.substring(11, refname.length);
        return branchlink(branch);
    }
    return refname;
}

function handleanyevent(ev, desc) {
    return [
        '<li><a href="https://github.com/',ev.actor.login,'">',ev.actor.login,'</a> ',
        desc,
        ' <span class="date">',printdate(ev.created_at),'</span></li>',
    ''].join('');
}

function remote_load() {
    var sc = document.createElement('script');
    sc.type = 'text/javascript';
    sc.src = 'https://api.github.com/repos/thomasmoelhave/tpie/events?callback=tpieevents';
    document.getElementsByTagName('head')[0].appendChild(sc);
}

function store_remote_events(input) {
    if (!localStorage) return false;
    var data = {input: input, cachetime: new Date().getTime()};
    localStorage.setItem('tpieevents', JSON.stringify(data));
}

function fetch_cached_remote_events() {
    if (!localStorage) return false;
    var str = localStorage.getItem('tpieevents');
    if (!str) return false;
    var data = JSON.parse(str);
    if (!data) return false;
    if (!data.input || !data.cachetime) return false;
    parse_events(data.input);
    var age = new Date().getTime() - data.cachetime;
    console.log("Age "+age);
    if (age < 0 || age > 30000) return false;
    return true;
}

$(function () {
    if (!fetch_cached_remote_events()) {
        console.log("cache miss");
        remote_load();
    } else {
        console.log("cache hit");
    }
});

// vim:set sw=4 sts=4 et: