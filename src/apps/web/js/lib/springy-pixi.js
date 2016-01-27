/**
Copyright (c) 2010 Dennis Hotson

 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:

 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
*/

(function() {

jQuery.fn.springyPixi = function(params) {
	var graph = this.graph = params.graph || new Springy.Graph();
	var nodeFont = "16px Verdana, sans-serif";
	var edgeFont = "8px Verdana, sans-serif";
	var stiffness = params.stiffness || 400.0;
	var repulsion = params.repulsion || 400.0;
	var damping = params.damping || 0.5;
	var minEnergyThreshold = params.minEnergyThreshold || 0.00001;
	var nodeSelected = params.nodeSelected || null;
	var nodeImages = {};
	var edgeLabelsUpright = true;

	var cnt = this[0];
	var pixi = PIXI.autoDetectRenderer($(cnt).width(), $(cnt).height(), {
		antialias: true,
		backgroundColor: 0xffffff
	});
	$(cnt).append(pixi.view)
	var stage = new PIXI.Container();

	var layout = this.layout = new Springy.Layout.ForceDirected(graph, stiffness, repulsion, damping, minEnergyThreshold);

	// calculate bounding box of graph layout.. with ease-in
	var currentBB = layout.getBoundingBox();
	var targetBB = {bottomleft: new Springy.Vector(-2, -2), topright: new Springy.Vector(2, 2)};

	$(window).resize(function () {
		console.log('123')
		pixi.resize($(cnt).width(), $(cnt).height());
	})

	// auto adjusting bounding box
	Springy.requestAnimationFrame(function adjust() {
		targetBB = layout.getBoundingBox();
		// current gets 20% closer to target every iteration
		currentBB = {
			bottomleft: currentBB.bottomleft.add( targetBB.bottomleft.subtract(currentBB.bottomleft)
				.divide(10)),
			topright: currentBB.topright.add( targetBB.topright.subtract(currentBB.topright)
				.divide(10))
		};

		Springy.requestAnimationFrame(adjust);
	});

	// convert to/from screen coordinates
	var toScreen = function(p) {
		var size = currentBB.topright.subtract(currentBB.bottomleft);
		var sx = p.subtract(currentBB.bottomleft).divide(size.x).x * $(cnt).width();
		var sy = p.subtract(currentBB.bottomleft).divide(size.y).y * $(cnt).height();
		return new Springy.Vector(sx, sy);
	};

	var fromScreen = function(s) {
		var size = currentBB.topright.subtract(currentBB.bottomleft);
		var px = (s.x / $(cnt).width()) * size.x + currentBB.bottomleft.x;
		var py = (s.y / $(cnt).height()) * size.y + currentBB.bottomleft.y;
		return new Springy.Vector(px, py);
	};

	// half-assed drag and drop
	var selected = null;
	var nearest = null;
	var dragged = null;

	jQuery(cnt).mousedown(function(e) {
		var pos = jQuery(this).offset();
		var p = fromScreen({x: e.pageX - pos.left, y: e.pageY - pos.top});
		selected = nearest = dragged = layout.nearest(p);

		if (selected.node !== null) {
			dragged.point.m = 10000.0;

			if (nodeSelected) {
				nodeSelected(selected.node);
			}
		}

		renderer.start();
	});

	// Basic double click handler
	jQuery(cnt).dblclick(function(e) {
		var pos = jQuery(this).offset();
		var p = fromScreen({x: e.pageX - pos.left, y: e.pageY - pos.top});
		selected = layout.nearest(p);
		node = selected.node;
		if (node && node.data && node.data.ondoubleclick) {
			node.data.ondoubleclick();
		}
	});

	jQuery(cnt).mousemove(function(e) {
		var pos = jQuery(this).offset();
		var p = fromScreen({x: e.pageX - pos.left, y: e.pageY - pos.top});
		nearest = layout.nearest(p);

		if (dragged !== null && dragged.node !== null) {
			dragged.point.p.x = p.x;
			dragged.point.p.y = p.y;
		}

		renderer.start();
	});

	jQuery(window).bind('mouseup',function(e) {
		dragged = null;
	});

	Springy.Node.prototype.getHeight = function() {
		return 30
	}

	Springy.Node.prototype.getWidth = function() {
		return 80
	}

	graph.addGraphListener({
		graphChanged: function (ev) {
			switch (ev.action) {
				case 'detachNode':
					if (ev.target.shape) {
						stage.removeChild(ev.target.shape)
					}
					break;
				case 'removeEdge':
					var edge = ev.target.connection
					var arrow = ev.target.arrow
					if (edge) stage.removeChild(edge)
					if (arrow) stage.removeChild(arrow)
					break;
			}
		}
	});

	var renderer = this.renderer = new Springy.Renderer(layout,
		function clear() {
			pixi.render(stage);
		},
		function drawEdge(edge, p1, p2) {
			var x1 = toScreen(p1).x;
			var y1 = toScreen(p1).y;
			var x2 = toScreen(p2).x;
			var y2 = toScreen(p2).y;

			var direction = new Springy.Vector(x2-x1, y2-y1);
			var normal = direction.normal().normalise();

			var from = graph.getEdges(edge.source, edge.target);
			var to = graph.getEdges(edge.target, edge.source);

			var total = from.length + to.length;

			// Figure out edge's position in relation to other edges between the same nodes
			var n = 0;
			for (var i=0; i<from.length; i++) {
				if (from[i].id === edge.id) {
					n = i;
				}
			}

			//change default to  10.0 to allow text fit between edges
			var spacing = 12.0;

			// Figure out how far off center the line should be drawn
			var offset = normal.multiply(-((total - 1) * spacing)/2.0 + (n * spacing));

			var paddingX = 6;
			var paddingY = 6;

			var s1 = toScreen(p1).add(offset);
			var s2 = toScreen(p2).add(offset);

			var boxWidth = edge.target.getWidth() + paddingX;
			var boxHeight = edge.target.getHeight() + paddingY;

			var intersection = intersect_line_box(s1, s2, {x: x2-boxWidth/2.0, y: y2-boxHeight/2.0}, boxWidth, boxHeight);

			if (!intersection) {
				intersection = s2;
			}

			var stroke = (edge.data.color !== undefined) ? edge.data.color : 0;

			var arrowWidth;
			var arrowLength;

			var weight = (edge.data.weight !== undefined) ? edge.data.weight : 1.0;
			var directional = (edge.data.directional !== undefined) ? edge.data.directional : true;

			var conn
			if (!edge.connection) {
				conn = edge.connection = new PIXI.Graphics()
				stage.addChild(conn);
			} else {
				conn = edge.connection
				conn.clear()
			}
			var lineWidth = Math.max(weight *  2, 0.1)
			conn.lineStyle(lineWidth, stroke)
			arrowWidth = 1 + lineWidth;
			arrowLength = 8;

			// line
			var lineEnd;
			if (directional) {
				lineEnd = intersection.subtract(direction.normalise().multiply(arrowLength * 0.5));
			} else {
				lineEnd = s2;
			}

			conn.beginFill();
			conn.moveTo(s1.x, s1.y);
			conn.lineTo(lineEnd.x, lineEnd.y);
			conn.endFill();

			var arrow
			if (!edge.arrow) {
				arrow = edge.arrow = new PIXI.Graphics()
				stage.addChild(arrow);
			} else {
				arrow = edge.arrow
				arrow.clear()
			}

			// arrow
			if (directional) {
				arrow.position.x = intersection.x
				arrow.position.y = intersection.y
				arrow.rotation = Math.atan2(y2 - y1, x2 - x1)
				arrow.lineStyle(lineWidth, stroke)
				arrow.beginFill();
				arrow.moveTo(-arrowLength, arrowWidth);
				arrow.lineTo(0, 0);
				arrow.lineTo(-arrowLength, -arrowWidth);
				arrow.lineTo(-arrowLength * 0.8, -0);
				arrow.endFill();
			}

			// label
			if (edge.data.label !== undefined) {
			return;
				text = edge.data.label
				ctx.save();
				ctx.textAlign = "center";
				ctx.textBaseline = "top";
				ctx.font = (edge.data.font !== undefined) ? edge.data.font : edgeFont;
				ctx.fillStyle = stroke;
				var angle = Math.atan2(s2.y - s1.y, s2.x - s1.x);
				var displacement = -8;
				if (edgeLabelsUpright && (angle > Math.PI/2 || angle < -Math.PI/2)) {
					displacement = 8;
					angle += Math.PI;
				}
				var textPos = s1.add(s2).divide(2).add(normal.multiply(displacement));
				ctx.translate(textPos.x, textPos.y);
				ctx.rotate(angle);
				ctx.fillText(text, 0,-2);
				ctx.restore();
			}

		},
		function drawNode(node, p) {
			if (!node.shape) {
				var shape = node.shape = new PIXI.Graphics()

				var paddingX = 6;
				var paddingY = 6;

				var contentWidth = node.getWidth();
				var contentHeight = node.getHeight();
				var boxWidth = contentWidth + paddingX;
				var boxHeight = contentHeight + paddingY;

				// fill background
				var fillColor
				if (selected !== null && selected.node !== null && selected.node.id === node.id) {
					fillColor = 0xFFFFE0;
				} else if (nearest !== null && nearest.node !== null && nearest.node.id === node.id) {
					fillColor = 0xEEEEEE;
				} else {
					fillColor = 0xFFFFFF;
				}
				shape.beginFill(fillColor)
				shape.lineStyle(1, 0, 1)
				shape.drawRect(- boxWidth/2, - boxHeight/2, boxWidth, boxHeight);
				shape.endFill()

				var txt = (node.data.label !== undefined) ? node.data.label : node.id;
				var text = node.text = new PIXI.Text(txt, {
					font: (node.data.font !== undefined) ? node.data.font : nodeFont,
					fill: (node.data.color !== undefined) ? node.data.color : 0
				})
				text.x = 0
				text.y = 0
				text.anchor.x = 0.5
				text.anchor.y = 0.5
				shape.addChild(text)
				stage.addChild(shape)
			}
			var s = toScreen(p);
			node.shape.position.x = s.x
			node.shape.position.y = s.y
		}
	);

	renderer.start();

	// helpers for figuring out where to draw arrows
	function intersect_line_line(p1, p2, p3, p4) {
		var denom = ((p4.y - p3.y)*(p2.x - p1.x) - (p4.x - p3.x)*(p2.y - p1.y));

		// lines are parallel
		if (denom === 0) {
			return false;
		}

		var ua = ((p4.x - p3.x)*(p1.y - p3.y) - (p4.y - p3.y)*(p1.x - p3.x)) / denom;
		var ub = ((p2.x - p1.x)*(p1.y - p3.y) - (p2.y - p1.y)*(p1.x - p3.x)) / denom;

		if (ua < 0 || ua > 1 || ub < 0 || ub > 1) {
			return false;
		}

		return new Springy.Vector(p1.x + ua * (p2.x - p1.x), p1.y + ua * (p2.y - p1.y));
	}

	function intersect_line_box(p1, p2, p3, w, h) {
		var tl = {x: p3.x, y: p3.y};
		var tr = {x: p3.x + w, y: p3.y};
		var bl = {x: p3.x, y: p3.y + h};
		var br = {x: p3.x + w, y: p3.y + h};

		var result;
		if (result = intersect_line_line(p1, p2, tl, tr)) { return result; } // top
		if (result = intersect_line_line(p1, p2, tr, br)) { return result; } // right
		if (result = intersect_line_line(p1, p2, br, bl)) { return result; } // bottom
		if (result = intersect_line_line(p1, p2, bl, tl)) { return result; } // left

		return false;
	}

	return this;
}

})();
