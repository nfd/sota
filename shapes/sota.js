var canvas;
var idxElem;
var script = [];
var logtextbox;

/* canvas bit-planes */
var bitplanes = new Array();
var colours = new Array("#0f0", "#00f", "#f00", "#ff0");

function setup() {
	canvas = document.getElementById("sotacanvas");
	idxElem = document.getElementById("idx");
	
	document.getElementById("step").addEventListener('click', step);

	idxElem.addEventListener('keypress', function(e) {
		if(e.keyCode == 13) {
			script_idx = idxElem.value;
			step();
		}
	});

	for(i = 0; i < 4; i++) {
		var bitplane = document.createElement("canvas");
		bitplane.width = canvas.width;
		bitplane.height = canvas.height;

		bitplanes.push(bitplane);
	}

	logtextbox = document.getElementById("commandstextbox");

	loadScript("script0013c6.json");
}

function log(idx, cmd, args) {
	var shortargs;

	if(args.hasOwnProperty("shape") && args["shape"].length > 5) {
		shortargs = JSON.stringify({"position": args["position"],
			"shape": args["shape"].slice(0, 5)}) + "[...]";
	} else {
		shortargs = JSON.stringify(args);
	}

	logtextbox.value += idx + ": " + cmd + ": " + shortargs + "\n";
	logtextbox.scrollTop = logtextbox.scrollHeight;
}

function clear(ctx) {
	ctx.clearRect(0, 0, canvas.width, canvas.height);
}

function draw_cmd_to_plane_idx(cmd) {
	switch(cmd) {
		default:
		case 0xd2:
			return 0;
			break;
		case 0xd3:
			return 1;
			break;
		case 0xd4:
			return 2;
			break;
		case 0xd5:
			return 3;
			break;
	}
}

function draw(bitplane, shape_info, clearPlane) {
	var shape = shape_info["shape"];

	// TODO combine bit-planes. Pretty difficult
	// to simulate without actually using bit-planes :(
	var ctx = bitplanes[bitplane].getContext("2d");

	ctx.strokeStyle = colours[bitplane];
	ctx.fillStyle = colours[bitplane];

	ctx.globalCompositeOperation='xor'; // default is source-over

	if(clearPlane) {
		clear(ctx);
	}

	if(shape.length > 0) {
		

		ctx.beginPath();
		ctx.moveTo(shape[0][0], shape[0][1]);

		for(var i = 1; i < shape.length; i++) {
			ctx.lineTo(shape[i][0], shape[i][1]);
		}

		//ctx.lineTo(shape[0][0] * 2, shape[0][1] * 2);
		//
		// window.requestAnimationFrame(callback)

		//ctx.lineWidth = 10.0;
		ctx.fill();
		ctx.closePath();
	}
}

function draw_multiple(ctx, idx, count) 
{
	// Read the next count cmds, which should be draws
	var drawn_in_plane = new Array(0, 0, 0, 0);

	while(count) {
		var cmd = script[idx][0];
		var args = script[idx][1];
		log(idx, cmd, args);

		if(cmd >= 0xd0 && cmd <= 0xdf) {
			// we know what these are.
			var bitplane = draw_cmd_to_plane_idx(cmd);

			draw(bitplane, args, drawn_in_plane[bitplane]==0);
			drawn_in_plane[bitplane] = 1;
			count --;
		} else if (cmd == 0) {
			// This appears to be a clear or reset which doesn't
			// affect the draw count following.
		} else if (cmd == 0xe6 || cmd == 0xe8) {
			// We don't know what these are, but they don't
			// cause problems so don't clutter the log.
			count --;
		} else {
			console.log("unexpected draw cmd " + cmd);
			console.log(args);
			count --;
		}
		idx ++;
	}

	clear(ctx);

	for(var i = 0; i < 4; i++) {
		ctx.drawImage(bitplanes[i], 0, 0);
	}

	return idx;
}

function step() {
	var idx = idxElem.value;

	var cmd = script[idx][0];
	var args = script[idx][1];
	log(idx, cmd, args);

	idx ++;

	var ctx = canvas.getContext("2d");

	switch(cmd) {
		case 0:
		case 1: // one vector follows
		case 2: // two vectors follow
		case 3: // three vectors follow
		case 4:
		case 5:
		case 6:
			idx = draw_multiple(ctx, idx, cmd);
			break;

		default:
			console.log(cmd + ": Unexpected cmd encountered");
			break;
	}

	idxElem.value = idx;
}

function loadScript(filename) {
	var req = new XMLHttpRequest();

	req.onreadystatechange = function() {
		if(req.readyState == 4 && req.status == 200) {
			script = JSON.parse(req.responseText);
			step();
		}
	}

	req.open("GET", filename, true);
	req.send();
}

