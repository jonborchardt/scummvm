/*global Module*/
Module["arguments"] = [];

// Add all parameters passed via the fragment identifier.
// With no fragment (the public cold-load path), boot straight into the
// bundled demo game: roger_no_launcher=true skips the Roger picker but a
// bare argv would still land in the stock ScummVM launcher.
if (window.location.hash.length > 0) {
    params = decodeURI(window.location.hash.substring(1)).split(" ")
    params.forEach((param) => {
        Module["arguments"].push(param);
    })
} else {
    Module["arguments"].push("betrayed");
}

// MIDI support
var midiOutputMap;
if (!("requestMIDIAccess" in navigator)) {
	console.error("No MIDI support in your browser.");
} else {
	navigator
		.requestMIDIAccess({ sysex: true, software: true })
		.then((midiAccess) => {
			midiOutputMap = midiAccess.outputs;
			midiAccess.onstatechange = (e) => {
				midiOutputMap = e.target.outputs;
			};
		});
}
