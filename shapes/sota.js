var canvas;
var idxElem;
var script = null;
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

	//loadScript("script0012bc.json");
	loadScript("script0c0374.json");
}

function log(idx, frame) {
	var text = "idx: " + idx + " cmd: " + frame[0] + "\n";

	for(i = 1; i < frame.length; i++) {
		var cmd = frame[i][0];
		var args = frame[i][1];
		var shortargs;

		if(args.hasOwnProperty("shape")) {
			shortargs = JSON.stringify({"position": args["position"],
				"shape": args["shape"].slice(0, 5)}) + "[...] len=" + args["shape"].length;
		} else {
			shortargs = JSON.stringify(args);
		}
		text += "  sub:" + cmd + " args:" + shortargs + "\n";
	}

	logtextbox.value += text;
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

function draw(bitplane, shape, clearPlane) {

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

function draw_multiple(ctx, frame) 
{
	// Read the next count cmds, which should be draws
	var drawn_in_plane = new Array(0, 0, 0, 0);

	var num_parts = frame[0];

	for(var i = 0; i < num_parts; i++) {
		var part = frame[i + 1];
		var cmd = part[0];
		var args = part[1];

		if(cmd >= 0xd0 && cmd <= 0xdf) {
			// we know what these are.
			var bitplane = draw_cmd_to_plane_idx(cmd);

			draw(bitplane, args["shape"], drawn_in_plane[bitplane]==0);
			drawn_in_plane[bitplane] = 1;
		} else if (cmd == 0xe6) {
			// Tweening
			var tween_shape_from = args[0];
			var tween_shape_to   = args[1];
			var tween_t          = args[2]; // position in tween
			var tween_count      = args[3];

			// Construct a temporary shape by lerping from shape_from to shape_to.
			var shape = lerp_tween(tween_shape_from, tween_shape_to, tween_t, tween_count, 0);
			draw(0, shape, drawn_in_plane[0]==0);
			drawn_in_plane[0] = 1;
		} else if (cmd == 0xe8) {
			// We don't know what this is, but they don't
			// cause problems so don't clutter the log.
			// Probably tweening in other bitplane.
		} else {
			console.log("unexpected draw cmd " + cmd);
			console.log(args);
		}
	}

	clear(ctx);

	for(var i = 0; i < 4; i++) {
		ctx.drawImage(bitplanes[i], 0, 0);
	}
}

function lerp_tween(tween_shape_from_idx, tween_shape_to_idx, tween_t, tween_count, bitplane_idx) {
	// okay this is a little nasty
	console.log(script["frames"][tween_shape_from_idx]);
	var tween_shape_from = script["frames"][tween_shape_from_idx][bitplane_idx + 1][1]["shape"];
	var tween_shape_to   = script["frames"][tween_shape_to_idx][bitplane_idx + 1][1]["shape"];
	var new_shape = new Array();

	// Point selection is wrong -- we should tween the points as well; if we go 10 to 20 we should
	// pick (0, 0), (0, 1), (1, 2), (2, 2) etc
	var points = Math.max(tween_shape_from.length, tween_shape_to.length);
	var point_multiplier_from = tween_shape_from.length / points;
	var point_multiplier_to   = tween_shape_to.length / points;

	var amt = tween_t / tween_count;

	for(var i = 0; i < points; i++) {
		var point_from = Math.floor(i * point_multiplier_from);
		var point_to   = Math.floor(i * point_multiplier_to);

		var x = Math.floor(tween_shape_from[point_from][0] + (amt * (tween_shape_to[point_to][0] - tween_shape_from[point_from][0])));
		var y = Math.floor(tween_shape_from[point_from][1] + (amt * (tween_shape_to[point_to][1] - tween_shape_from[point_from][1])));
		new_shape.push([x, y]);
	}

	return new_shape;

}

function step() {
	var idx = idxElem.value;

	if(idx < script["indices"].length) {
		var frame = script["frames"][script["indices"][idx]];

		log(idx, frame);

		var ctx = canvas.getContext("2d");
		draw_multiple(ctx, frame);

		idx ++;
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

