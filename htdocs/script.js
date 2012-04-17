var socket = null, disconnected = true;

start("ws://" + document.location.host + "/foobar");

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
				default:
					if (msg.data != undefined) writeToScreen('msg='+msg.data);
					break;
			}
		}
	} else { /* No WebSocket support */
		writeToScreen('You web browser does not seem to support WebSocket. :(');
	}
}

function doSend(message)
{
	writeToScreen("SENT: " + message);
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

function init()
{
}
