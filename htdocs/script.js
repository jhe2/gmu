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
				writeToScreen(": Disconnected from server. Reconnecting...");
				disconnected = true;
			}
			//try to reconnect in 2 seconds
			setTimeout('start("'+websocketServerLocation+'")', 2000);
		};

		socket.onopen = function()
		{
			writeToScreen(": Socket has been opened!");
			disconnected = false;
		}

		socket.onmessage = function(msg)
		{
			var jmsg = JSON.parse(msg.data);

			switch (jmsg['cmd']) {
				case 'time':
					writeToTimeDisplay(jmsg['time']);
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
					writeToScreen('Track:' + jmsg['artist'] + ' - ' + jmsg['title']);
					setTrackInfo(jmsg['artist'], jmsg['title'], jmsg['album']);
					break;
				case 'playlist_info':
				case 'playlist_change':
					t = document.getElementById("playlisttable");
					rows = jmsg['length'];
					pl.length = 0;
					pl_set_number_of_items(rows);
					writeToTimeDisplay("pl length="+rows);
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
							if (!pl[pos]) {
								doSend("playlist_get_item:"+pos);
								break;
							}
						}
					}
					//writeToScreen(jmsg['position']);
					break;
				default:
					if (msg.data != undefined) writeToScreen('msg='+msg.data);
					break;
			}
		}
	} else { /* No WebSocket support */
		writeToScreen('Your web browser does not seem to support WebSockets. :(');
	}
}

function doSend(message)
{
	//writeToScreen("SENT: " + message);
	socket.send(message);
}

function writeToScreen(message)
{
	var output = document.getElementById('log');
	var pre = document.createElement("p");
	pre.style.wordWrap = "break-word";
	pre.innerHTML = message;
	output.appendChild(pre);
}

function writeToTimeDisplay(message)
{
	var output = document.getElementById('time');
	/*var pre = document.createElement("p");
	pre.style.wordWrap = "break-word";
	pre.innerHTML = message;*/
	output.innerHTML = message;
}

function setTrackInfo(artist, title, album)
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
	doSend("play:"+id);
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
				if (!pl[first_visible_pl_line+j]) {
					if ((r = t.rows[j])) {
						r.childNodes[0].innerHTML = first_visible_pl_line+j+1;
						r.childNodes[1].innerHTML = '?';
						r.childNodes[2].innerHTML = '?';				
					}
				}
				j++;
			} while (j <= visible_pl_line_count);
			doSend("playlist_get_item:"+(first_visible_pl_line+i));
		}
	}
	//writeToScreen("fvl="+first_visible_pl_line+" scrolltop="+document.getElementById('plscrollbar').scrollTop);
}

function mouse_scroll_event_handler(e)
{
	var evt = window.event || e; // equalize event object
	var delta = evt.detail ? evt.detail * (-120) : evt.wheelDelta; // delta returns +120 when wheel is scrolled up, -120 when scrolled down
	dir = (delta <= -120) ? 20 : -20;
	document.getElementById('plscrollbar').scrollTop += dir;
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
	//writeToTimeDisplay("height="+height);
	pl_item_height = plt.clientHeight;
	visible_pl_line_count = parseInt(height / pl_item_height) + 1;
	for (i = 0; i < visible_pl_line_count; i++)
		add_row("playlisttable", '', '?', '', '#111');
}

function init()
{
	var mousewheelevt = (/Firefox/i.test(navigator.userAgent))? "DOMMouseScroll" : "mousewheel"; // FF doesn't recognize mousewheel as of FF3.x

	elem = document.getElementById('pl');
	if (elem.attachEvent) // if IE (and Opera depending on user setting)
		elem.attachEvent("on"+mousewheelevt, mouse_scroll_event_handler);
	else if (elem.addEventListener) // W3C browsers
		elem.addEventListener(mousewheelevt, mouse_scroll_event_handler, false);
	init_pl_table();
	start("ws://" + document.location.host + "/foobar");
	//doSend("playlist_get_info");
	window.onresize = function(event) {
		init_pl_table();
		handle_playlist_scroll();
	}
}
