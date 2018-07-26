/*
	player_ui.js

	Manages front-end behaviour
*/

"use strict";

const POLL_INTERVAL_MS = 1000;
const UPDATE_TIMEOUT = 5000;
const ERROR_DISPLAY_TIME = 2000;

var socket = io.connect();

var lastUpdateTimeNodejs = Date.now();

// Run when webpage fully loaded
$(document).ready(function() {

	// Register callback functions for each button
	$('#btn-addsong').click(function() { submitSongLink(); });

	$('#btn-playpause').click(	function() { sendPlayPause(); });
	$('#btn-skipsong').click(	function() { sendSkipSong(); });

	$('#btn-vol-up').click(	 function() { sendVolumeUp(); });
	$('#btn-vol-down').click(function() { sendVolumeDown(); });

	// Incoming control messages
	socket.on('serverReply', function(data) {
		handleServerCommands(data);
	});

	// Poll server for new data
	pollServer();
});


//
// Sending commands to the server
//========================================================================

const CMD_VOLUME_UP   = "volup";
const CMD_VOLUME_DOWN = "voldown";
const CMD_PLAY        = "play";
const CMD_PAUSE       = "pause";
const CMD_SKIP        = "skip";
const CMD_ADD_SONG    = "addsong=";
const CMD_REMOVE_SONG = "rmsong=";
const CMD_REPEAT_SONG = "repeat=";

function sendServerCommand(data) {
	socket.emit('clientCommand', 'cmd\n' + data + '\n');
};


//
// Song Queue
//========================================================================

// This 
var songQueue = [];

// Submits input link to server
function submitSongLink() {
	// Get form input
	var songUrl = $('#new-song-input').val();

	// Clear form input
	$('#new-song-input').val("");

	var videoId = youtube_parser(songUrl);
	if (!videoId) {
		// TODO: Show error invalid link
		setError("Cannot add invalid YouTube link!")
		return;
	}

	// Send to server
	sendServerCommand(CMD_ADD_SONG + videoId);
}

var apiKey = "AIzaSyAZkC1t4CApwcbyk-JOTVxe5QQVHfblw9g";

// Adds a song to end of the list
function addSong(videoId) {
	// Check Youtube link
	$.get("https://www.googleapis.com/youtube/v3/videos?id=" + videoId + "&key=" + apiKey + "&part=snippet,contentDetails", 
		function(data) {
			// Get youtube video title
			var videoTitle = data.items[0].snippet.title;

			// Parse duration
			var videoDuration = data.items[0].contentDetails.duration;
			var durationSeconds = parseDuration(videoDuration);

			var songItem = {
				"id": videoId,
				"title": videoTitle,
				"duration": durationSeconds,
			};

			songQueue.push(songItem);

			refreshSongTableHtml();	
	});
}


// Finds the input URL in current list, then removes it
function removeSong(videoId) {
	console.log("Removing", videoId);
	// Find the song url
	for (var i = 0; i < songQueue.length; i++) {
		var song = songQueue[i];
		if (song.id == videoId) {
			// Remove at index
			songQueue.splice(i, 1);
			break;
		}
	}

	// If it exists, remove it and update the table HTML
}

function emptyQueue(shouldUpdateDisplay=false) {
	songQueue = [];
	if (shouldUpdateDisplay) {
		refreshSongTableHtml();	
	}
}

var deferNum = 0;

function refreshSongTableHtml() {
	// Sometimes, we don't want to refresh nultiple times
	deferNum--;
	if (deferNum > 0) {
		return;
	}

	var newTableHtml = `
<tr>
	<th></th>
	<th>TITLE</th>
	<th>TIME</th> 
	<th>...</th>
</tr>
	`;

	if (songQueue.length == 0) {		
		const newRowHtmlString = `
<tr>
	<td></td>
    <td>Nothing in queue!</td>
    <td></td>
    <td></td>
</tr>
		`;
		newTableHtml = newTableHtml + newRowHtmlString;
	}
	else {
		for (var i = 0; i < songQueue.length; i++) {
			var song = songQueue[i];
			
			// Add class for first song
			var currentlyPlayingClass = "";
			var playingIcon = "";
			if (i == 0) {
				currentlyPlayingClass = " class=\"song-currently-playing\"";
				playingIcon = "<i class=\"fas fa-play\"></i>";
			}



			const newRowHtmlString = `
<tr${currentlyPlayingClass}>
	<td>${playingIcon}</td>
	<td><a href="https://www.youtube.com/watch?v=${song.id}">${song.title}</a></td>
	<td>${parseSecsToString(song.duration)}</td>
	<td>
		<span href="#" onclick="removeSong('${song.id}');"><i class="fas fa-trash"></i></span>
	</td>
</tr>
			`;
			newTableHtml = newTableHtml + newRowHtmlString;
		}
	}

	$("#song-list").html(newTableHtml);
}

// Calls deferRefreshSongTableHtml only when counter reaches num
function deferRefreshSongTableHtml(num) {
	deferNum = num;
}

// Taken from https://stackoverflow.com/questions/3452546/how-do-i-get-the-youtube-video-id-from-a-url
function youtube_parser(url){
    var regExp = /^.*((youtu.be\/)|(v\/)|(\/u\/\w\/)|(embed\/)|(watch\?))\??v?=?([^#\&\?]*).*/;
    var match = url.match(regExp);
    return (match&&match[7].length==11)? match[7] : false;
}

var prevQueueData = "undefined";
function handleSongQueueData(data) {
	// Update things only if data is different
	if (data == prevQueueData) {
		return;
	}

	prevQueueData = data;

	emptyQueue();

	// Queue data is video IDs separated by delimiters
	var videoId;
	var videoIds = data.split(',');
	var numValidVids = 0;

	for (videoId of videoIds) {
		if (videoId.length > 2) {
			addSong(videoId);
			numValidVids++;
		}
	}

	// Call refresh when all songs have been added
	deferRefreshSongTableHtml(numValidVids);
}


//
// Playback control
//========================================================================

var isPlaying = false;
function sendPlayPause() {
	if (isPlaying) {
		sendServerCommand(CMD_PAUSE)
	}
	else {
		sendServerCommand(CMD_PLAY)
	}
	setPlayPauseDisplay(!isPlaying);
}


function setPlayPauseDisplay(isPlayingInput) {
	isPlaying = isPlayingInput;
	
	// Change play/puase button display
	if (isPlaying) {
		$("#btn-playpause").attr("class", "fas fa-pause-circle fa-4x");
	}
	else {
		$("#btn-playpause").attr("class", "fas fa-play-circle fa-4x");
	}
}


function sendSkipSong() {
	sendServerCommand(CMD_SKIP);
}


//
// Volume functions
//========================================================================

function sendVolumeUp() {
	sendServerCommand(CMD_VOLUME_UP); 
}

function sendVolumeDown() {
	sendServerCommand(CMD_VOLUME_DOWN); 
}

function setDisplayVolume(newVolume) {
	$('#volumeId').attr("value", parseInt(newVolume));
}


function pollServer() {
	socket.emit('clientCommand', 'statusping\n');
	window.setTimeout(pollServer, POLL_INTERVAL_MS);

	if ((Date.now() - lastUpdateTimeNodejs) > UPDATE_TIMEOUT) {
		// setError("No response from Node.js server. Is it running?")
	}
}

//
// Song time progress
//========================================================================

const defaultTimeDisplay = "-:-- / -:--";
const minutes_re = /(\d+)M/
const seconds_re = /(\d+)S/

// Parses the duration string from Youtube API
function parseDuration(durationStr) {
	var minutes = durationStr.match(minutes_re);
	var seconds = durationStr.match(seconds_re);

	var totalSeconds = 0;
	if (minutes && minutes[1]) {
		totalSeconds += parseInt(minutes[1]) * 60;
	}
	if (seconds && seconds[1]) {
		totalSeconds += parseInt(seconds[1]);
	}

	return totalSeconds;
}

function parseSecsToString(totalSeconds) {
	var mins = String(Math.floor(totalSeconds / 60));
	var secs = String(totalSeconds % 60);

	var str;
	if (secs.length == 1) {
		str = `${mins}:0${secs}`;
	}
	else {
		str = `${mins}:${secs}`;
	}

	return str;
}

// Updates the time display for currently playing song
// Input is a fraction 0 - 1
function setSongProgress(progressAmount) {
	if (!songQueue[0]) {
		$('#volumeId').attr("value", defaultTimeDisplay);
		return;
	}

	var totalTime = parseSecsToString(songQueue[0].duration); 
	var currentTime = parseSecsToString(parseInt(songQueue[0].duration * parseFloat(progressAmount))); 

	var timeDisplayStr = `${currentTime} / ${totalTime}`;
	$('#song-progress-time').html(timeDisplayStr);	
}


//
// Handling server commands
//========================================================================

const COMMANDS_DELIM = /[;\n]/;
const COMMAND_DELIM = /[ =]/;

// Handles multiple commands seperated by COMMAND_DELIM
function handleServerCommands(data) {
	var commands = data.split(COMMANDS_DELIM);
	for (var i in commands) {
		handleServerCommand(commands[i]);
	}

	lastUpdateTimeNodejs = Date.now();
}

// Handles single command
function handleServerCommand(command) {
	var parsedWords = command.split(COMMAND_DELIM);
	if (parsedWords.length == 0) {
		return;
	}

	var primaryCommand = parsedWords[0];
	var subCommand = parsedWords[1];

	switch (primaryCommand) {
		case "play":
			setPlayPauseDisplay(true);
			break;

		case "pause":
			setPlayPauseDisplay(false);
			break;

		case "vol":
			setDisplayVolume(subCommand);
			break;

		case "progress":
			setSongProgress(subCommand);

		case "queue":
			handleSongQueueData(subCommand);
			break;

		default:
			console.log("Error: Unrecognized command %s", primaryCommand);
	}
}

var errorTimeout;
function setError(errorMsg) {
	$("#error-text").html(errorMsg);
	$('#error-box').show();

	clearTimeout(errorTimeout);
	errorTimeout = window.setTimeout(function() { $('#error-box').hide(); }, ERROR_DISPLAY_TIME);
}
