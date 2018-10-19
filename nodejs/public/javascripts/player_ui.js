/*
	player_ui.js

	Manages front-end behaviour
*/

"use strict";

const POLL_INTERVAL_MS = 500;
const UPDATE_TIMEOUT = 5000;
const ERROR_DISPLAY_TIME = 2000;

var socket = io.connect();

var serverResponded = true;
var lastResponseTime = Date.now();

var apiKey = "AIzaSyAZkC1t4CApwcbyk-JOTVxe5QQVHfblw9g";

// Run when webpage fully loaded
$(document).ready(function() {

	// Register callback functions for each button
    // index.html
	$("#new-song-input").keyup(function(event) {
	    if (event.keyCode === 13) {
            $("#btn-addsong").click();
	    }
	});

    // addsong.html
	$('#btn-searchsong').click(function() { submitSongSearch(); });

    // playback buttons
    $('#btn-volume').click(	function() { volumeClick(); });
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
});


//
// Error display
//========================================================================

var errorTimeout;

function setError(errorMsg) {
	$("#error-text").html(errorMsg);
	$('#error-box').show();

	clearTimeout(errorTimeout);
	errorTimeout = window.setTimeout(function() { $('#error-box').hide(); }, ERROR_DISPLAY_TIME);
}


//
// Sending commands to the server
//========================================================================

const CMD_VOLUME_UP   = "volup";
const CMD_VOLUME_DOWN = "voldown";
const CMD_VOLUME 	  = "vol=";
const CMD_PLAY        = "play";
const CMD_PAUSE       = "pause";
const CMD_SKIP        = "skip";
const CMD_ADD_SONG    = "addsong=";
const CMD_REMOVE_SONG = "rmsong=";
const CMD_REPEAT_SONG = "repeat=";

function sendServerCommand(data) {
	socket.emit('clientCommand', data + '\n');
};

function pollServer() {
    if (serverResponded) {
        socket.emit('clientCommand', 'statusping\n');
        window.setTimeout(pollServer, POLL_INTERVAL_MS);
        serverResponded = false;
    } else if ((Date.now() - lastResponseTime) > UPDATE_TIMEOUT) {
        setError("No response from the server. Is it running?");
    }
}


//
// Searching for songs
//========================================================================

const SEARCH_MAX_RESULTS = 10;

function addSearchedSong(videoId) {
}

// Searches for YouTube videos that match the user's query
function querySongs() {
    var searchQuery = $('#tb-song-search').val();

    // Test if search query contains at least one alphanumeric character
    if (!/[a-zA-Z0-9]/.test(searchQuery)) {
        return;
    }

    // Build the query
    var query = "?q=" + encodeURIComponent(searchQuery);
    query += "&key=" + apiKey;
    query += "&maxResults=" + SEARCH_MAX_RESULTS;
    query += "&type=video";
    query += "&part=snippet";

    // Send a search request to Google/YouTube
	$.get("https://www.googleapis.com/youtube/v3/search" + query,
        function(search_data) {
            if (search_data.items.length == 0) {
                $('#search-results-title').val("Search Results: \"" + query + "\"");
                $('#search-results-list').val("<h4>No results</h4>");
                return;
            }

            // Send another request to get the duration of each video
            var ids = [];

            for (let item of search_data.items) {
                ids.push(item.id.videoId);
            }

            $.get("https://www.googleapis.com/youtube/v3/videos?part=contentDetails&id=" + ids.join() + "&key=" + apiKey,
                function(details_data) {
                    // Reset the title and search result contents
                    $('#search-results-title').html("Search results for \"" + searchQuery + "\"");
                    $('#search-results-list').html("");

                    // Set HTML for search results
                    for (let item of search_data.items) {
                        // Find the duration of this video
                        var duration = "Unknown length";
                        for (var j = 0; j < details_data.items.length; j++) {
                            if (details_data.items[j].id === item.id.videoId) {
                                duration = parseSecsToString(parseDuration(details_data.items[j].contentDetails.duration));
                            }
                        }

                        var html = `<div class="row search-results-item">
<div class="col-6 col-md-3 thumbnail-container">
<img class="thumbnail" src="` + item.snippet.thumbnails.default.url + `">
</div>
<div class="col-6 col-md-6">
<h4><a href="https://www.youtube.com/watch?v=` + item.id.videoId + `" target="_blank">` + item.snippet.title + `</a></h4>
<p><i class="fas fa-address-card"></i> ` + item.snippet.channelTitle + `</p>
<p><i class="fas fa-stopwatch"></i> ` + duration + ` </p>
</div>
<div class="col-12 col-md-3 button-container">
<button id="btn-` + item.id.videoId + `" class="search-results-item-button btn btn-outline-primary" type="button">
<i class="fas fa-plus-circle"></i> Add to queue
</button>
</div>
</div>`;

                        $('#search-results-list').append(html);

                        // Add a click callback for the add to queue button
                        $('#btn-' + item.id.videoId).click(
                            function () {
                                sendServerCommand(CMD_ADD_SONG + item.id.videoId);
                                window.location.href = "index.html";
                        });
                    }

                    // Show results list div if it isn't already displayed
                    $('#search-results-box').show();
            });
    });
}


//
// Song Queue
//========================================================================

var songQueue = [];

// Submits input link to server
function submitSongSearch() {
	// Get form input
	var songUrl = $('#tb-song-search').val();

    // First, check if it is a valid YouTube link
	var videoId = youtube_parser(songUrl);
    if (videoId) {
        sendServerCommand(CMD_ADD_SONG + videoId);
        window.location.href = "index.html";
        return;
    }

    // If it isn't a valid YouTube link, perform a search
    querySongs();
}

// Adds a song to end of the list
function addSong(videoId, index) {
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
                "index": index,
			};

            // Insert the song details in the correct place in the queue
            if (songQueue.length > 0) {
                for (var i = 0; i < songQueue.length; i++) {
                    if (songQueue[i].index > songItem.index) {
                        songQueue.splice(i, 0, songItem);
                        break;
                    }
                }
            } else {
			    songQueue.push(songItem);
            }

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
    return (match&&match[7].length==11) ? match[7] : false;
}

var prevQueueData = "undefined";
function handleSongQueueData(data, appendData=false) {
	if (!appendData) {
		// Update things only if data is different
		if (data == prevQueueData) {
			return;
		}

		prevQueueData = data;

		emptyQueue();
	}


	// Queue data is video IDs separated by delimiters
	var videoId;
	var videoIds = data.split(',');
	var numValidVids = 0;

	for (videoId of videoIds) {
		if (videoId.length > 2) {
			addSong(videoId, numValidVids);
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
	
	// Change play/pause button display
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
var volumePressed = false;
function volumeClick() {
    volumePressed = !volumePressed;

    if(volumePressed) {
        $("#btn-volume").append(
        	'<div class="volume-popup">' +
			'<input id="vol-control" type="range" min="0" max="100" step="1" oninput="sendVolumeValue(this.value)" onchange="sendVolumeValue(this.value)" style="position:absolute;right:40px;">' +
			'</div>');


    }
    else {
        $('.volume-popup').remove();

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

function sendVolumeValue(val) {
	sendServerCommand(CMD_VOLUME + val);
}

function setDisplayVolume(newVolume) {
	$('#volume-display-val').html(parseInt(newVolume));
	$("#vol-control").attr("value", newVolume);
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
	if (songQueue.length == 0) {
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
// Handling server commands
//========================================================================

const CMD_RESPONSE_PLAY      = "play";
const CMD_RESPONSE_VOL       = "vol";
const CMD_RESPONSE_REPEAT    = "repeat";
const CMD_RESPONSE_STATUS    = "status";
const CMD_RESPONSE_PROGRESS  = "progress";
const CMD_RESPONSE_QUEUE     = "queue";
const CMD_RESPONSE_QUEUE_EXT = "queuemore";

const RESPONSE_CMDS_DELIM = /[;\n]/;
const RESPONSE_CMD_DELIM = /[ =]/;

// Handles multiple commands seperated by RESPONSE_CMDS_DELIM
function handleServerCommands(data) {
	var commands = data.split(RESPONSE_CMDS_DELIM);
	for (var i in commands) {
		handleServerCommand(commands[i]);
	}

	lastResponseTime = Date.now();
    serverResponded = true;
}

// Handles single command
function handleServerCommand(command) {
	var parsedWords = command.split(RESPONSE_CMD_DELIM);
	if (parsedWords.length == 0) {
		return;
	}

	var primaryCommand = parsedWords[0];
	var subCommand = parsedWords[1];

	switch (primaryCommand) {
		case CMD_RESPONSE_PLAY:
			setPlayPauseDisplay(subCommand == '1');
			break;

		case CMD_RESPONSE_VOL:
			setDisplayVolume(subCommand);
			break;

		case CMD_RESPONSE_REPEAT:
			break;

		case CMD_RESPONSE_STATUS:
			handleSongStatus(subCommand)
			break;

		case CMD_RESPONSE_PROGRESS:
			setSongProgress(subCommand);
			break;

		case CMD_RESPONSE_QUEUE:
			handleSongQueueData(subCommand);
			break;

		case CMD_RESPONSE_QUEUE_EXT:
			handleSongQueueData(subCommand, true);
			break;

		default:
			console.log("Error: Unrecognized command %s", primaryCommand);
	}
}
