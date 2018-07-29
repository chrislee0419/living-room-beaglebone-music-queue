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
	$("#new-song-input").keyup(function(event) {
	    if (event.keyCode === 13) {
	        $("#btn-addsong").click();
	    }
	});

	$('#btn-addsong').click(function() { submitSongLink(); });

	$('#btn-repeat').click(	function() { sendRepeatSong(); });
	$('#btn-playpause').click(	function() { sendPlayPause(); });
	$('#btn-skipsong').click(	function() { sendSkipSong(); });

	$('#btn-vol-up').click(	 function() { sendVolumeUp(); });
	$('#btn-vol-down').click(function() { sendVolumeDown(); });

	$('#btn-savesettings').click(function() { saveSettings(); });

	// Incoming control messages
	socket.on('serverReply', function(data) {
		handleServerCommands(data);
	});

	// Poll server for new data
	pollServer();

	handleModeChange();
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
const CMD_CHANGE_MODE = "mode";

function sendServerCommand(data) {
	socket.emit('clientCommand', data + '\n');
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
			var statusIcon = "";
			if (i == 0) {
				currentlyPlayingClass = " class=\"song-currently-playing\"";
				statusIcon = `<i id=\"song-status-icon\" class="${getStatusIconClass()}"></i>`;
			}

			const newRowHtmlString = `
<tr${currentlyPlayingClass}>
	<td>${statusIcon}</td>
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
	if (numValidVids == 0) {
		refreshSongTableHtml();
	}
	else {
		deferRefreshSongTableHtml(numValidVids);
	}
}


const SONG_STATUS_UNKNOWN     = -1;
const SONG_STATUS_QUEUED      = 0;
const SONG_STATUS_LOADING     = 1;
const SONG_STATUS_LOADED      = 2;
const SONG_STATUS_REMOVED     = 3;
const SONG_STATUS_PLAYING     = 4;

var currentSongStatus = SONG_STATUS_UNKNOWN;
function handleSongStatus(statusData) {
	var newSongStatus = parseInt(statusData);
	if (currentSongStatus != newSongStatus)
	{
		currentSongStatus = newSongStatus;
		if (currentSongStatus != SONG_STATUS_UNKNOWN) {
			$("#song-status-icon").attr("class", getStatusIconClass());
		}
	}
}

function getStatusIconClass() {
	var iconClass = "";
	switch(currentSongStatus) {
		case SONG_STATUS_LOADING:
			iconClass = "fas fa-spinner fa-spin";
			break;
		case SONG_STATUS_PLAYING:
			iconClass = "fas fa-play";
			break;
	}

	return iconClass;
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

// true for playing
function setPlayPauseDisplay(isPlayingInput) {
	isPlaying = isPlayingInput;
	
	// Change play/puase button display
	if (isPlaying) {
		$("#btn-playpause").attr("class", "fas fa-pause-circle fa-4x playback-icon");
	}
	else {
		$("#btn-playpause").attr("class", "fas fa-play-circle fa-4x playback-icon");
	}
}


function sendSkipSong() {
	sendServerCommand(CMD_SKIP);
}


var isRepeating = false;
function sendRepeatSong() {
	isRepeating = !isRepeating;

	if(isRepeating) {
		sendServerCommand(CMD_REPEAT_SONG + '1');
		$("#btn-repeat").addClass("playback-icon-selected").removeClass("playback-icon");
	}
	else {
		sendServerCommand(CMD_REPEAT_SONG + '0');
		$("#btn-repeat").addClass("playback-icon").removeClass("playback-icon-selected");
	}
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
	$('#volumeId').html(parseInt(newVolume));
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
// Input is a string "123/435" of the fraction representing the progress
function setSongProgress(progressInput) {
	if (!songQueue[0]) {
		$('#song-progress-time').html(defaultTimeDisplay);
		return;
	}

	var nums = progressInput.split('/');
	var progressFraction;
	if (nums[0] == '0' || nums[1] == '0') {
		progressFraction = 0;
	}
	else {
		progressFraction = parseFloat(nums[0]) / parseFloat(nums[1]);
	}	

	var totalTime = parseSecsToString(songQueue[0].duration); 
	var currentTime = parseSecsToString(Math.round(songQueue[0].duration * parseFloat(progressFraction))); 

	var timeDisplayStr = `${currentTime} / ${totalTime}`;
	$('#song-progress-time').html(timeDisplayStr);
}


//
// Master/slave device connection settings
//========================================================================

const DEVICE_MODE_MASTER = "master";
const DEVICE_MODE_SLAVE = "slave";

var deviceMode = DEVICE_MODE_MASTER;

function getServerMode() {
	sendServerCommand(CMD_GET_MODE);
}

function setDeviceMode(newMode) {
	deviceMode = newMode;
	if (deviceMode == DEVICE_MODE_MASTER) {
		$('#radioModeMaster').prop("checked", true);
		$('#radioModeSlave').prop("checked", false);
	}
	else {
		$('#radioModeMaster').prop("checked", false);
		$('#radioModeSlave').prop("checked", true);
	}

	handleModeChange();
}

// Displays the corresponding IP address (master) or IP input field (slave)
function handleModeChange() {
	if($('#radioModeMaster').is(':checked')) {
		deviceMode = DEVICE_MODE_MASTER;
		$('#connectIpFormGroup').hide();
		$('#showIpFormGroup').show();
	}
	else {
		deviceMode = DEVICE_MODE_SLAVE;
		$('#connectIpFormGroup').show();
		$('#showIpFormGroup').hide();
	}
}

function saveSettings() {
	if (deviceMode == DEVICE_MODE_MASTER) {
		sendServerCommand(CMD_CHANGE_MODE + deviceMode);
	}
	else {
		var addressInput = $('#inputMasterIp').val();
		sendServerCommand(CMD_CHANGE_MODE + deviceMode + ',' + addressInput);
	}
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
			setPlayPauseDisplay(subCommand == '1');
			break;

		case "vol":
			setDisplayVolume(subCommand);
			break;

		case "repeat":
			break;

		case "status":
			handleSongStatus(subCommand)
			break;

		case "progress":
			setSongProgress(subCommand);
			break;

		case "mode":
			setDeviceMode(subCommand);
			break;

		case "queue":
			handleSongQueueData(subCommand);
			break;

		case "queuemore":
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
