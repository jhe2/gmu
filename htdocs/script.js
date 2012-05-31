/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: script.js  Created: 120213
 *
 * Description: HTTP/Websocket frontend
 */
var socket = null, disconnected = true;
var visible_pl_line_count = 0, first_visible_pl_line = 0;
var pl_item_height = 1;
var pl = [];
var selected_tab = 'pl';

window.onload = function() { init(); }

function start(websocketServerLocation)
{	
	if (typeof WebSocket != 'undefined')
		socket = new WebSocket(websocketServerLocation);
	else if (typeof MozWebSocket != 'undefined')
		socket = new MozWebSocket(websocketServerLocation);

	if (socket) {
		socket.onclose = function()
		{
			if (!disconnected) {
				write_to_screen(": Disconnected from server. Reconnecting...");
				disconnected = true;
			}
			//try to reconnect in 2 seconds
			setTimeout('start("'+websocketServerLocation+'")', 2000);
		};

		socket.onopen = function()
		{
			write_to_screen(": Socket has been opened!");
			disconnected = false;
		}

		socket.onmessage = function(msg)
		{
			var jmsg = JSON.parse(msg.data);

			switch (jmsg['cmd']) {
				case 'time':
					write_to_time_display(jmsg['time']);
					break;
				case 'playback_state':
					switch(jmsg['state']) {
						case 0: // stop
							document.getElementById("btn-play").className = "button";
							document.getElementById("btn-pause").className = "button";
							break;
						case 1: // play
							document.getElementById("btn-play").className = "button-pressed";
							document.getElementById("btn-pause").className = "button";
							break;
						case 2: // pause
							document.getElementById("btn-pause").className = "button-pressed";
							document.getElementById("btn-play").className = "button";
							break;
					}
					break;
				case 'trackinfo':
					write_to_screen('Track:' + jmsg['artist'] + ' - ' + jmsg['title']);
					set_trackinfo(jmsg['artist'], jmsg['title'], jmsg['album']);
					break;
				case 'playlist_info':
				case 'playlist_change':
					t = document.getElementById("playlisttable");
					rows = jmsg['length'];
					pl_set_number_of_items(rows);
					write_to_time_display("pl length="+rows);
					toggle_state = false;
					current_length = t.rows.length;
					for (id = 0; r = t.rows[id]; id++) {
						r.childNodes[1].innerHTML = '!';
						r.childNodes[2].innerHTML = '?';
					}
					handle_playlist_scroll();
					break;
				case 'playlist_item':
					pl[jmsg['position']] = jmsg['title'];
					t = document.getElementById("playlisttable");
					r = t.rows[jmsg['position']-first_visible_pl_line];
					if (r) {
						r.childNodes[0].innerHTML = jmsg['position'];
						r.childNodes[1].innerHTML = "<a href=\"javascript:play("+jmsg['position']+");\">" + 
									"<img src=\"music.png\" width=\"16\" height=\"16\" alt=\"\" border=\"0\" /> " +
									jmsg['title'] + "</a>";
						r.childNodes[2].innerHTML = '?';
						for (i = 0; i <= visible_pl_line_count; i++) {
							pos = jmsg['position']+1+i;
							if (pos < pl.length && !pl[pos]) {
								do_send("playlist_get_item:"+pos);
								break;
							} else if (pos >= pl.length) {
								break;
							}
						}
					}
					break;
				default:
					if (msg.data != undefined) write_to_screen('msg='+msg.data);
					break;
			}
		}
	} else { /* No WebSocket support */
		write_to_screen('Your web browser does not seem to support WebSockets. :(');
	}
}

function do_send(message)
{
	//write_to_screen("SENT: " + message);
	if (socket) socket.send(message);
}

function write_to_screen(message)
{
	var output = document.getElementById('log');
	var pre = document.createElement("p");
	pre.style.wordWrap = "break-word";
	pre.innerHTML = message;
	output.appendChild(pre);
}

function write_to_time_display(message)
{
	var output = document.getElementById('time');
	/*var pre = document.createElement("p");
	pre.style.wordWrap = "break-word";
	pre.innerHTML = message;*/
	output.innerHTML = message;
}

function set_trackinfo(artist, title, album)
{
	document.getElementById('ti-artist').innerHTML = artist;
	document.getElementById('ti-title').innerHTML  = title;
	document.getElementById('ti-album').innerHTML  = album;
}

function select_tab(tab_id)
{
	elem = document.getElementsByName('tab');
	for (i = 0; i < elem.length; i++)
		elem[i].style.display = "none";
	elem = document.getElementsByName('tabitem');
	for (i = 0; i < elem.length; i++)
		elem[i].className = "inactive";
	document.getElementById('t'+tab_id).className = "active";
	document.getElementById(tab_id).style.display = "block";
	selected_tab = tab_id;
}

function add_row(table_id, col1, col2, col3, bg)
{
	var tabl = document.getElementById(table_id);
	var ro   = tabl.insertRow(tabl.rows.length);
	ro.style.backgroundColor = bg;
	cell1 = ro.insertCell(0);
	cell1.innerHTML = col1;
	cell1.style.width = "5%";
	cell1.style.height = "20px";
	cell2 = ro.insertCell(1);
	cell2.innerHTML = col2;
	cell2.style.width = "90%";
	cell2.style.height = "20px";
	cell3 = ro.insertCell(2);
	cell3.innerHTML = col3;
	cell3.style.width = "5%";
	cell3.style.height = "20px";
}

function play(id)
{
	do_send("play:"+id);
}

function pl_set_number_of_items(items)
{
	pl.length = rows;
	document.getElementById("plscrolldummy").style.height = "" + (items*pl_item_height) + "px";
}

function init_pl_table()
{
	document.getElementById("playlisttable").innerHTML = '';
	add_row("playlisttable", '?', '?', '?', '#111');
	var pl = document.getElementById('pl');
	var plt = document.getElementById("playlisttable");
	var height = pl.clientHeight;
	//write_to_time_display("height="+height);
	pl_item_height = plt.clientHeight;
	visible_pl_line_count = parseInt(height / pl_item_height) + 1;
	for (i = 0; i < visible_pl_line_count; i++)
		add_row("playlisttable", '', '?', '', '#111');
}

function add_event_handler(elem_id, event, event_handler)
{
	elem = document.getElementById(elem_id);
	if (elem.attachEvent) // if IE (and Opera depending on user setting)
		elem.attachEvent("on"+event, event_handler);
	else if (elem.addEventListener) // W3C browsers
		elem.addEventListener(event, event_handler, false);
}

function pl_scroll_n_rows(n)
{
	document.getElementById('plscrollbar').scrollTop += (20 * n);
}

function handle_playlist_scroll()
{
	first_visible_pl_line = parseInt(document.getElementById('plscrollbar').scrollTop / pl_item_height);
	t = document.getElementById("playlisttable");
	for (i = 0; i <= visible_pl_line_count; i++) {
		if (pl[first_visible_pl_line+i]) {
			if ((r = t.rows[i])) {
				r.childNodes[0].innerHTML = first_visible_pl_line+i;
				r.childNodes[1].innerHTML = "<a href=\"javascript:play("+(first_visible_pl_line+i)+");\">" + 
							"<img src=\"music.png\" width=\"16\" height=\"16\" alt=\"\" border=\"0\" /> " +
							pl[first_visible_pl_line+i] + "</a>";
				r.childNodes[2].innerHTML = '?';
			}
		} else {
			var j = i;
			do {
				if (!pl[first_visible_pl_line+j] && (r = t.rows[j])) {
					if (first_visible_pl_line+j+1 < pl.length) {
						r.childNodes[0].innerHTML = first_visible_pl_line+j+1;
						r.childNodes[1].innerHTML = '?';
						r.childNodes[2].innerHTML = '?';
					} else {
						r.childNodes[0].innerHTML = '';
						r.childNodes[1].innerHTML = '';
						r.childNodes[2].innerHTML = '';
					}
				}
				j++;
			} while (j <= visible_pl_line_count);
			if (first_visible_pl_line+i < pl.length)
				do_send("playlist_get_item:"+(first_visible_pl_line+i));
		}
	}
	//write_to_screen("fvl="+first_visible_pl_line+" scrolltop="+document.getElementById('plscrollbar').scrollTop);
}

function handle_mouse_scroll_event(e)
{
	var evt = window.event || e; // equalize event object
	var delta = evt.detail ? evt.detail * (-120) : evt.wheelDelta; // delta returns +120 when wheel is scrolled up, -120 when scrolled down
	dir = (delta <= -120) ? 1 : -1;
	pl_scroll_n_rows(dir);
}

function handle_btn_next(e)
{
	do_send('next');
}

function handle_btn_prev(e)
{
	do_send('prev');
}

function handle_btn_play(e)
{
	do_send('play');
}

function handle_btn_pause(e)
{
	do_send('pause');
}

function handle_btn_stop(e)
{
	do_send('stop');
}

function handle_tab_select_fb(e)
{
	var evt = window.event || e;
	select_tab('fb');
}

function handle_tab_select_pl(e)
{
	var evt = window.event || e;
	select_tab('pl');
}

function handle_tab_select_log(e)
{
	var evt = window.event || e;
	select_tab('lo');
}

function handle_keypress(e)
{
	// Page up: 33, down: 34, Crsr down: 40, up: 38
	switch (selected_tab) {
		case 'pl':
			if (e.keyCode == 40)      // Cursor down
				pl_scroll_n_rows(1);
			else if (e.keyCode == 38) // Cursor up
				pl_scroll_n_rows(-1);
			break;
		default:
			break;
	}
}

function init()
{
	var mwevt = (/Firefox/i.test(navigator.userAgent))? "DOMMouseScroll" : "mousewheel"; // FF doesn't recognize mousewheel as of FF3.x

	document.onkeydown = handle_keypress;
	add_event_handler('pl',          mwevt,    handle_mouse_scroll_event);
	add_event_handler('btn-next',    'click',  handle_btn_next);
	add_event_handler('btn-prev',    'click',  handle_btn_prev);
	add_event_handler('btn-play',    'click',  handle_btn_play);
	add_event_handler('btn-pause',   'click',  handle_btn_pause);
	add_event_handler('btn-stop',    'click',  handle_btn_stop);
	add_event_handler('plscrollbar', 'scroll', handle_playlist_scroll);
	add_event_handler('tfb',         'click',  handle_tab_select_fb);
	add_event_handler('tpl',         'click',  handle_tab_select_pl);
	add_event_handler('tlo',         'click',  handle_tab_select_log);
	init_pl_table();
	start("ws://" + document.location.host + "/foobar");
	//do_send("playlist_get_info");
	window.onresize = function(event) {
		init_pl_table();
		handle_playlist_scroll();
	}
}
