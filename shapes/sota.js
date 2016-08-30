var canvas;
var idxElem;
var indices = null;
var data = null;
var logtextbox;
var animating = 0;
var animation_start_ms;
var last_animation_frame = -1;

var ms_per_frame = 50;

/* canvas bit-planes */
var bitplanes = new Array();
var bpdata = new Array();
var viewport_stride;
var bpstride = new Array(); // some bitplanes (the backgrounds) are larger than the viewport.
var bpwidth = new Array();
var bpheight = new Array();
var bpoffset = new Array(); // start index for bitplane (to allow for scrolling)

/*
var palette = new Array(0xff000000, 0xff000000, 0xff007711, 0xff0000dd, 0xff770077, 0xff0077dd, 0xff111177, 0xff00bbbb,
		0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000);
		*/

// debug palette
var palette = new Array(0xff000000, 0xffffffff, 0xff007711, 0xff0000dd, 0xff770077, 0xff0077dd, 0xff111177, 0xff00bbbb,
		0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000);


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

	// problematic points: 209, 195, 19, 8, 101
	//idxElem.value = "0";

	// other anim: 34, 37
	idxElem.value = "0";

	viewport_stride = canvas.width;

	var bitplane_width = canvas.width;
	var bitplane_height = canvas.height;

	for(i = 0; i < 4; i++) {
		if(i == 1) {
			// Double the size of the bitplanes so we can use them for moving backgrounds.
			bitplane_width *= 2;
			bitplane_height *= 2;
		}

		var bitplane = new ArrayBuffer(bitplane_width * bitplane_height);
		bitplanes.push(bitplane);

		var ctx = new Uint8Array(bitplane);
		bpdata.push(ctx);

		bpstride.push(bitplane_width);
		bpwidth.push(bitplane_width);
		bpheight.push(bitplane_height);
		bpoffset.push(0);
	}

	logtextbox = document.getElementById("commandstextbox");

	//loadScript("script010800.json");
	//loadScript("script067080.json");
	//loadScript("script07789e.json");
	//loadScript("script075c66.json");
	loadScript("script08da00.json");
	//loadScript("script078104.json");
}

function log(text) {
	logtextbox.value += text + "\n";
	logtextbox.scrollTop = logtextbox.scrollHeight;
}

function draw_thick_circle(xc, yc, radius, thickness, bitplane_idx) {
	var width = bpwidth[bitplane_idx];
	var height = bpheight[bitplane_idx];
	var stride = bpstride[bitplane_idx];
	var pixels = bpdata[bitplane_idx];
	var pen = 1 << bitplane_idx;

	if(radius <= 0)
		return;

	function putpixel(px, py) {
		if(px >= 0 && px < width && py >= 0 && py <= height)
			pixels[py * stride + px] = pen;
	}

	var xouter = radius + thickness;
    var xinner = radius;
    var y = 0;
    var errouter = 1 - xouter;
    var errinner = 1 - xinner;

	function horiz_line(y, x1, x2) {
		// TODO: slow
		while (x1 <= x2) putpixel(x1++, y);
	}

	function vert_line(x, y1, y2) {
		// TODO: slow
		while (y1 <= y2) putpixel(x, y1++);
	}


	while(xouter >= y) {
		horiz_line(yc + y, xc + xinner, xc + xouter);
		vert_line(xc + y,  yc + xinner, yc + xouter);
		horiz_line(yc + y, xc - xouter, xc - xinner);
		vert_line(xc - y,  yc + xinner, yc + xouter);
		horiz_line(yc - y, xc - xouter, xc - xinner);
		vert_line(xc - y,  yc - xouter, yc - xinner);
		horiz_line(yc - y, xc + xinner, xc + xouter);
		vert_line(xc + y,  yc - xouter, yc - xinner);

		y++;

		if (errouter < 0) {
			errouter += 2 * y + 1;
		} else {
			xouter --;
			errouter += 2 * (y - xouter + 1);
		}

		if (y > radius) {
			xinner = y;
		} else {
			if (errinner < 0) {
				errinner += 2 * y + 1;
			} else {
				xinner --;
				errinner += 2 * (y - xinner + 1);
			}
		}
	}
}

function background_init_concentric_circles() {
	for(var radius = 10; radius < 700; radius+= 12) {
		draw_thick_circle(Math.floor(bpwidth[1] / 2), Math.floor(bpheight[1] / 2), radius, 6, 1);
		draw_thick_circle(Math.floor(bpwidth[2] / 2), Math.floor(bpheight[2] / 2), radius, 6, 2);
	}
}

function clear_bitplane(idx) {
	var longView = new Uint32Array(bitplanes[idx]);
	for(var i = 0; i < (canvas.width * canvas.height / 4); i ++) {
		longView[i] = 0;
	}
}

function draw_cmd_to_plane_idx(cmd) {
	switch(cmd) {
		default:
		case 0xd2:
			return 0;
			break;
		case 0xd4:
			return 1;
			break;
	}
}

function draw_polyfill(bitplane_idx, data, base_idx, length) {
	/* Polygon fill algorithm */
	// The following tutorial was most helpful:
	// https://www.cs.uic.edu/~jbell/CourseNotes/ComputerGraphics/PolygonFilling.html
	// This tutorial cleared up some corner cases:
	// http://web.cs.ucdavis.edu/~ma/ECS175_S00/Notes/0411_b.pdf

	// Line info entry is [xmin, ymax, 1/m, previous-idx, ymin]
	// line info is indexed from the edge table: edge table for a given y co-ordinate (ymin)
	// is an index into the line_info table.
	
	var line_info = new Array();
	var edge_table = new Array(256);
	var i;
	var global_ymin = 256;
	var global_ymax = 0;
	var bitplane = bpdata[bitplane_idx];
	var fill_value = 1 << bitplane_idx;

	/* Fill edge_table and line_info*/
	for(i=0; i < length * 2; i+= 2) {
		var y0, x0, y1, x1;

		y0 = data[base_idx + i];
		x0 = data[base_idx + i + 1];

		if(i == (length * 2) - 2) {
			// close the shape
			y1 = data[base_idx];
			x1 = data[base_idx + 1];
		} else {
			y1 = data[base_idx + i + 2];
			x1 = data[base_idx + i + 3];
		}

		if(y0 == y1) {
			// Horizontal edge; just ignore it.
			continue;
		}

		// ensure y0 <= y1
		if(y0 > y1) {
			var tmp;
			tmp = y0; y0 = y1; y1 = tmp;
			tmp = x0; x0 = x1; x1 = tmp;
		}

		if(y0 < global_ymin)
			global_ymin = y0; // highest point of the polygon.

		if(y1 > global_ymax) 
			global_ymax = y1;

		line_info.push([x0, y1, (x1 - x0) / (y1 - y0), edge_table[y0], y0, x0]);
		edge_table[y0] = line_info.length - 1;
	}

	/*
	for(i = global_ymin; i < global_ymax; i++) {
		var idx = edge_table[i];

		while(line_info[idx] != undefined) {
			log(i + " " + line_info[idx]);
			idx = line_info[idx][3];
		}
	}
	*/

	// Active edge table: subset of the edge table which is currently being drawn.
	var active_lines = new Array();

	// Scaline algorithm.
	for(var y = global_ymin; y <= global_ymax; y++) {
		// Move all edges with ymin == y into the active line table.
		var line_info_idx = edge_table[y];
		while(line_info_idx != undefined) {
			active_lines.push(line_info[line_info_idx]);
			line_info_idx = line_info[line_info_idx][3];
		}

		// Sort the active edge table on xmin 
		active_lines.sort(function(lhs, rhs) {
			// wtf that JS requires this, but it does
			if(lhs[0] < rhs[0]) return -1;
			if(lhs[0] > rhs[0]) return 1;
			return 0;
		});

		// Draw the lines.
		var is_drawing = 0;
		var prev_x = 0;
		for(i = 0; i < active_lines.length; i++) {
			var next_x = is_drawing ? Math.floor(active_lines[i][0]) : Math.ceil(active_lines[i][0]);

			var is_vertex = Math.abs(active_lines[i][5] - active_lines[i][0]) < 0.1
				&& (active_lines[i][1] == y || active_lines[i][4] == y);

			/*
			if(y == 70) {
				fill_value = 3;
				log(y + ": drawing " + prev_x + " to " + next_x +" (y0=" + active_lines[i][4] + ", y1=" + active_lines[i][1] + "): " + is_drawing + " is vertex " + is_vertex);
			} else {
				fill_value = 1<<bitplane_idx;
			}
			*/

			if(is_drawing) {
				var startXIdx = (canvas.width * y) + prev_x;
				var endXIdx = (canvas.width * y) + next_x;
				for(var x = startXIdx; x < endXIdx; x++) {
					bitplane[x] ^= fill_value;
				}
			}

			if(!is_vertex) {
				is_drawing = 1 - is_drawing;
			/*} else if (active_lines[i][1] == active_lines[i-1][1] 
					|| active_lines[i][4] == active_lines[i-1][4]) {*/
			} else if(active_lines[i][1] == y) {
				is_drawing = 1 - is_drawing;
			}


			prev_x = next_x;
		}

		// Remove active edges where ymax == y
		i = 0;
		while(i < active_lines.length) {
			if(active_lines[i][1] == y) {
				active_lines.splice(i, 1);
			} else {
				i++;
			}
		}
		

		// Update the gradients in AET.
		for(i = 0; i < active_lines.length; i++) {
			active_lines[i][0] += active_lines[i][2];
		}
	}
}

function draw(bitplane_idx, data, idx, clearPlane) {

	var length = data[idx++];

	if(clearPlane) {
		clear_bitplane(bitplane_idx);
	}

	if(length > 0) {
		/*
		ctx.strokeStyle = "#0f0";
		ctx.fillStyle = "#0f0";
		draw_standard(ctx, data, idx, length);
		*/

		draw_polyfill(bitplane_idx, data, idx, length);

		idx += (length * 2);
	}

	return idx;
}
function combine_bitplanes(ctx)
{
	// straight out of https://hacks.mozilla.org/2011/12/faster-canvas-pixel-manipulation-with-typed-arrays/
	var imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
	var buf = new ArrayBuffer(imageData.data.length);
	var buf8 = new Uint8ClampedArray(buf);
	var data = new Uint32Array(buf);

	var idx = 0;
	var bpidx = [bpoffset[0], bpoffset[1], bpoffset[2], bpoffset[3]];
	
	for(var y = 0; y < canvas.height; y++) {
		for(var x = 0; x < canvas.width; x++) {
			var colour = bpdata[0][bpidx[0] + x] 
				| bpdata[1][bpidx[1] + x]
				| bpdata[2][bpidx[2] + x]
				| bpdata[3][bpidx[3] + x];

			data[idx + x] = palette[colour];
		}

		bpidx[0] += bpstride[0];
		bpidx[1] += bpstride[1];
		bpidx[2] += bpstride[2];
		bpidx[3] += bpstride[3];
		idx += viewport_stride;
	}

	imageData.data.set(buf8);
	ctx.putImageData(imageData, 0, 0);
}

function draw_multiple(ctx, idx) 
{
	// Read the next count cmds, which should be draws
	var drawn_in_plane = new Array(0, 0, 0, 0);

	var num_draws = data[idx++];

	for(var i = 0; i < num_draws; i++) {
		var cmd = data[idx++];

		log("draw cmd " + cmd.toString(16) + " idx " + (idx - 2) + " len " + data[idx]);
		if(cmd >= 0xd0 && cmd <= 0xdf) {
			// we know what these are.
			var bitplane_idx = 0;

			idx = draw(bitplane_idx, data, idx, drawn_in_plane[bitplane_idx]==0);
			drawn_in_plane[bitplane_idx] = 1;
		} else if (cmd == 0xe6 || cmd == 0xe7 || cmd == 0xe8 || cmd == 0xf2) {
			var bitplane_idx = 0; //cmd == 0xe6? 0: 2;

			var current_position = idx;

			var tween_from  = current_position - ((data[idx++] << 8) + (data[idx++]));
			var tween_to    = current_position + ((data[idx++] << 8) + (data[idx++]));
			var tween_t     = data[idx++]; // position in tween
			var tween_count = data[idx++];

			//log("Tween " + tween_from + " " + tween_to + " " + tween_t + " " + tween_count);

			var shape = lerp_tween(tween_from, tween_to, tween_t, tween_count);
			draw(bitplane_idx, shape, 0, drawn_in_plane[bitplane_idx]==0);
			drawn_in_plane[bitplane_idx] = 1;
		} else {
			console.log("unexpected draw cmd " + cmd);
			console.log(args);
		}
	}

	//ctx.clearRect(0, 0, canvas.width, canvas.height);
	combine_bitplanes(ctx);
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

	bpoffset[2] = bpstride[2] * Math.floor((100 + 90 * Math.sin(5.0 + idx / 25))) + 50;

	// TODO
	bpoffset[1] = Math.floor((100 + 90 * Math.sin(1.0 + idx / 20))) + (50 * bpstride[1])
		+ bpstride[1] * (Math.floor((60 + 90 * Math.sin(4.0 + idx / 25))));


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

			//background_init_concentric_circles();
			step();
		}
	}

	req.open("GET", filename, true);
	req.send();
}

