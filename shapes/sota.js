var canvas;
var idxElem;
var indices = null;
var data = null;
var logtextbox;
var animating = 0;
var animation_start_ms;
var last_animation_frame = -1;

var ms_per_frame = 33;

/* canvas bit-planes */
var bitplanes = new Array();
var bpcontext = new Array();
var colours = new Array("#0f0", "#00f", "#f00", "#ff0");

function setup() {
	canvas = document.getElementById("sotacanvas");
	idxElem = document.getElementById("idx");
	
	document.getElementById("step").addEventListener('click', step);
	document.getElementById("animate").addEventListener('click', start_animating);

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

		var ctx = bitplane.getContext("2d");

		ctx.strokeStyle = "#fff";
		ctx.fillStyle = "#fff";

		ctx.globalCompositeOperation='xor'; // default is source-over
		bpcontext.push(ctx);
	}

	logtextbox = document.getElementById("commandstextbox");

	//loadScript("script0012bc.json");
	loadScript("script067230.json");
	//loadScript("script09e1b8.json");
}

function log(text) {
	/*
	logtextbox.value += text + "\n";
	logtextbox.scrollTop = logtextbox.scrollHeight;
	*/
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

function draw(bitplane, data, idx, clearPlane) {

	var ctx = bpcontext[bitplane];

	if(clearPlane) {
		clear(ctx);
	}

	var length = data[idx++];

	if(length > 0) {
		var x, y;

		ctx.beginPath();
		y = data[idx++];
		x = data[idx++];

		ctx.moveTo(x, y);

		for(var i = 1; i < length; i++) {
			y = data[idx++];
			x = data[idx++];
			ctx.lineTo(x, y);
		}

		//ctx.stroke();
		ctx.closePath();
		ctx.fill();
	}

	return idx;
}

function draw_multiple(ctx, idx) 
{
	// Read the next count cmds, which should be draws
	var drawn_in_plane = new Array(0, 0, 0, 0);

	var num_draws = data[idx++];

	for(var i = 0; i < num_draws; i++) {
		var cmd = data[idx++];

		//log("draw cmd " + cmd.toString(16) + " idx " + (idx - 2) + " len " + data[idx]);
		if(cmd >= 0xd0 && cmd <= 0xdf) {
			// we know what these are.
			var bitplane = draw_cmd_to_plane_idx(cmd);

			idx = draw(bitplane, data, idx, drawn_in_plane[bitplane]==0);
			drawn_in_plane[bitplane] = 1;
		} else if (cmd == 0xe6 || cmd == 0xe8) {
			var bitplane = cmd == 0xe6? 0: 2;

			var current_position = idx;

			var tween_from  = current_position - ((data[idx++] << 8) + (data[idx++]));
			var tween_to    = current_position + ((data[idx++] << 8) + (data[idx++]));
			var tween_t     = data[idx++]; // position in tween
			var tween_count = data[idx++];

			var shape = lerp_tween(tween_from, tween_to, tween_t, tween_count);
			draw(bitplane, shape, 0, drawn_in_plane[bitplane]==0);
			drawn_in_plane[bitplane] = 1;
		} else if (cmd == 0xf2) {
			// ???
			idx += 6;
		} else {
			console.log("unexpected draw cmd " + cmd);
			console.log(args);
		}
	}

	clear(ctx);

	for(var i = 0; i < 4; i++) {
		if(drawn_in_plane[i]) {
			ctx.drawImage(bitplanes[i], 0, 0);
		}
	}
}

function lerp_tween(tween_from_idx, tween_to_idx, tween_t, tween_count) {
	// We expect these to be draw commands (i.e. 0xd2) followed by lengths.
	if(data[tween_from_idx] & 0xd0 != 0xd0) {
		console.log("Unexpected tween idx (from)");
	}

	if(data[tween_to_idx] & 0xd0 != 0xd0) {
		console.log("Unexpected tween idx (to)");
	}

	var tween_from_length = data[tween_from_idx + 1];
	var tween_to_length   = data[tween_to_idx + 1];

	var tween_from_data = tween_from_idx + 2;
	var tween_to_data   = tween_to_idx + 2;

	// new_shape will just be a length followed by a set of (y, x) points.
	var new_shape = new Array();

	var points = Math.max(tween_from_length, tween_to_length);
	new_shape.push(points);

	var point_multiplier_from = tween_from_length / points;
	var point_multiplier_to   = tween_to_length / points;

	var amt = tween_t / tween_count;

	for(var i = 0; i < points; i++) {
		var point_from = Math.floor(i * point_multiplier_from);
		var point_to   = Math.floor(i * point_multiplier_to);

		var idx_from   = tween_from_data + (point_from * 2);
		var idx_to     = tween_to_data + (point_to * 2);

		var y = Math.floor(data[idx_from] + (amt * (data[idx_to] - data[idx_from])));
		new_shape.push(y);

		var x = Math.floor(data[idx_from + 1] + (amt * (data[idx_to + 1] - data[idx_from + 1])));
		new_shape.push(x);
	}

	return new_shape;

}

function step() {
	var idx;
	var shouldDraw = true;

	if(animating) {
		idx = Math.floor((Date.now() - animation_start_ms) / ms_per_frame);
		shouldDraw = idx != last_animation_frame;
	} else {
		idx = idxElem.value;
	}

	if(idx < indices.length) {
		//log(idx, frame);

		if(shouldDraw) {
			var ctx = canvas.getContext("2d");
			draw_multiple(ctx, indices[idx]);
		}

		if(animating) {
			last_animation_frame = idx;
			window.requestAnimationFrame(step);
		} else {
			idx ++;
		}

	} else {
		animating = false;
	}

	idxElem.value = idx;
}

function start_animating() {
	idxElem.value = "0";

	animating = true;
	last_animation_frame = -1;
	animation_start_ms = Date.now();
	step();
}

function loadScript(filename) {
	var req = new XMLHttpRequest();

	req.onreadystatechange = function() {
		if(req.readyState == 4 && req.status == 200) {
			var script = JSON.parse(req.responseText);
			indices = script["indices"];
			data = script["data"];
			step();
		}
	}

	req.open("GET", filename, true);
	req.send();
}

