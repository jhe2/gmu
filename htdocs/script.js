var socket, disconnected = true;

start("ws://" + document.location.hostname + ":" + document.location.port + "/foobar");

function start(websocketServerLocation)
{
	socket = new WebSocket(websocketServerLocation);
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
		var foo = JSON.parse(msg.data);

		if (foo['cmd'] == 'time') {
			writeToTimeDisplay(foo['time']);
		} else if (foo['cmd'] == 'playback_state') {
			switch(foo['state']) {
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
		} else {
			writeToScreen(msg.data);
		}
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

