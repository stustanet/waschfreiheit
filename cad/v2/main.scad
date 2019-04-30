// If you experience problems with not having ssn/ssn.scad install it from
// https://gitlab.stusta.de/stustanet/ssn-openscad and follow instructions there.
use<ssn/ssn.scad>

pcb_size = [ 88, 75, 30 ];
$fn          = 50;
sidewall     = 3;
bottomwall   = 2;

hook_pos_1 = 20;
hook_pos_2 = 90;
hook_z     = 2.5;

frontyard    = 20;
top_overhead = 5;
outer_size   = pcb_size + [ sidewall * 2 + 10, sidewall * 2 + frontyard, -12 ];
top_size     = [ outer_size[0], outer_size[1], outer_size[2] ] +
           [ sidewall * 2, sidewall + top_overhead, 18 ];

mounting_holes = [
	[ -5, 5, 0 ],
	[ -5, pcb_size[1] + 5, 0 ],
	[ pcb_size[0] - 5, pcb_size[1] + 5, 0 ]
];

num_leds        = 5;
led_identifiers = [ 2, 7, 10, 16, 18 ];

module led_digit(num) {
	sl      = 5;  // Segment length
	sw      = 1;  // Segment width
	sh      = 4;  // Segment height
	numbers = [
		[ true, true, true, true, true, true, false ],
		[ false, true, true, false, false, false, false ],
		[ true, true, false, true, true, false, true ],
		[ true, true, true, false, false, true, true ],
		[ false, true, true, false, false, true, true ],
		[ true, false, true, true, false, true, true ],
		[ true, false, true, true, true, true, true ],
		[ true, true, true, false, false, false, false ],
		[ true, true, true, true, true, true, true ],
		[ true, true, true, true, false, true, true ]
	];

	spec = numbers[num];

	if (spec[0])
		translate([ 0, 2 * (sl - sw), 0 ]) cube([ sl, sw, sh ]);
	if (spec[1])
		translate([ sl - sw, sl - sw, 0 ]) cube([ sw, sl, sh ]);
	if (spec[2])
		translate([ sl - sw, 0, 0 ]) cube([ sw, sl, sh ]);
	if (spec[3])
		translate([ 0, 0, 0 ]) cube([ sl, sw, sh ]);
	if (spec[4])
		translate([ 0, 0, 0 ]) cube([ sw, sl, sh ]);
	if (spec[5])
		translate([ 0, sl - sw, 0 ]) cube([ sw, sl, sh ]);
	if (spec[6])
		translate([ 0, sl - sw, 0 ]) cube([ sl, sw, sh ]);
}

module led_number(number) {
	letter_width    = 5;
	letter_distance = 2;
	rotate([ 0, 0, 180 ]) {
		if (number >= 10) {
			translate([ -letter_width * 2 - letter_distance, 0, 0 ])
			    led_digit(number / 10);

			translate([ -letter_width * 1, 0, 0 ]) led_digit(number % 10);
		} else {
			translate([ -letter_width, 0, 0 ]) led_digit(number);
		}
	}
}

module pcb_holes(diameter1 = 1.2, diameter2 = 1.2) {
	translate([ 13.5 + 2.7 / 2, 25.5 + 2.7 / 2, 0 ])
	    cylinder(h = 1, d1 = diameter1, d2 = diameter2);

	translate([ 2 + 2.7 / 2, 51 + 2.7 / 2, 0 ])
	    cylinder(h = 1, d1 = diameter1, d2 = diameter2);

	translate([ 55 + 2.7 / 2, 48.4 + 2.7 / 2, 0 ])
	    cylinder(h = 1, d1 = diameter1, d2 = diameter2);

	translate([ 83.2 + 2.7 / 2, 1.7 + 2.7 / 2, 0 ])
	    cylinder(h = 1, d1 = diameter1, d2 = diameter2);
}

module led_holes() {
	translate([ 21, 85, top_size[2] - 2 ]) {
		for (i = [0:num_leds - 1]) {
			translate([ 0, -i * 12.7, 0 ]) led_number(led_identifiers[i]);
		}
	}
}

module led_spacer() {
	translate([ 21, 85, top_size[2] - 2 ]) {
		for (i = [1:num_leds - 1]) {
			translate([ 0, -i * 12.7 + 0.5, -5 ]) cube([ 10, 2, 6 ]);
		}
	}
}

module pcb() {
	cube(pcb_size);

	translate([ -5.5, pcb_size[1] - 5, 7 ]) rotate([ 90, 0, 0 ]) {
		translate([ 2.5, 0, 0.5 ]) rotate([ 0, 90, 0 ]) cylinder(d = 1, h = 5);

		cylinder(d = 5.5, h = 40);
	}
}

module hook(orientation, type, depth, width, length, height) {
	scale([ 1, orientation, 1 ]) {
		if (type == 1) {
			// Mount
			translate([ 0, -depth, 0 ]) {
				translate([ -width / 2, 0, -width / 2 ]) {
					cube([ length, depth, width ]);

					translate([ 0, 0, width ]) intersection() {
						cube([ length, depth, depth ]);
						rotate([ -45, 0, 0 ])
						    cube([ length, depth * 2, depth * 2 ]);
					}
				}

				translate([ length - width * 1.5, 0, -width / 2 ])
				    cube([ width, depth, height ]);

				translate([ length - width, depth / 2, height - width / 2 ])
				    rotate([ 0, 45, 0 ])
				        cube([ width * 2, depth, width * 2 ], center = true);
			}
		} else if (type == 2) {
			// Hook (round)
			translate([ 0, 0, width / 2 - depth - 0.5 ]) rotate([ 90, 0, 0 ])
			    hull() {
				// union() {
				translate([ 0, 0, depth - 0.1 ]) cylinder(d = width, h = 0.1);

				translate([ 0, -depth + 0.1, 0 ]) cylinder(d = width, h = 0.1);

				translate([ 0, depth - 0.1, 0 ]) cylinder(d = width, h = 0.1);
			}
		} else if (type == 3) {
			// Hook (Upside down U shape)
			translate([ 0, 0, width / 2 - depth - 0.5 ]) rotate([ 90, 0, 0 ])
			    hull() {
				// union() {
				translate([ 0, 0, depth - 0.1 ])
				    cylinder(d = width - 0.3, h = 0.1);

				translate([ 0, -width / 4, depth / 2 ])
				    cube([ width - 0.3, width / 2, depth ], center = true);

				translate([ 0, depth - 0.1, 0 ])
				    cylinder(d = width - 0.3, h = 0.1);
			}
		} else {
			// hook (square)
			cube([ width, depth, width ], center = true);
			translate([ -width / 2, -depth / 2, width / 2 ]) intersection() {
				cube([ width, depth, width ]);
				rotate([ -45, 0, 0 ]) cube([ length, depth * 2, depth * 2 ]);
			}
		}
	}
}

module cableholder(diameters, width, offset) {
	// width = (len(diameters) + 2) * 10;
	difference() {
		cube([ width, 12, 5 ]);
		for (i = [0:len(diameters) - 1]) {
			translate([ offset + 15 + 10 * i, 0, 0 ]) rotate([ -90, 0, 0 ])
			    cylinder(d = diameters[i], h = 15);
		}
	}

	for (i = [0:len(diameters) - 1]) {
		translate([ offset + 15 + 10 * i, 5, diameters[i] * 0.6 ])
		    rotate([ 45, 0, 0 ])
		        cube([ diameters[i], diameters[i] / 2, diameters[i] / 2 ],
		             center = true);
	}
}

module base() {
	difference() {
		union() {
			difference() {
				union() {
					translate([ -sidewall - 10, -sidewall, -bottomwall ])
					    cube(outer_size);
				}
				pcb();

				translate([ 0, 0, -2 ]) scale([ 1, 1, 5 ]) pcb_holes();

				// Make a border to the antenna-area
				translate([ -10, 0, 0 ]) cube([ 9, 75, 20 ]);
				translate([ -2, 0, 14 ]) cube([ 2, 75, 20 ]);
				translate([ -2, 67, 0 ]) cube([ 2, 10, 20 ]);

				// Make some room for the frontyard
				translate([ -10, pcb_size[1] - 0.1, 0 ])
				    cube([ pcb_size[0] + 10, frontyard + 5, 30 ]);

				// Add some hooks
				translate([ -10 - sidewall - 0.1, hook_pos_1, hook_z ])
				    rotate([ 0, 0, 270 ]) hook(-1, 1, 2, 5, 15, 18);

				translate([ -10 - sidewall - 0.1, hook_pos_2, hook_z ])
				    rotate([ 0, 0, 270 ]) hook(-1, 1, 2, 5, 15, 18);

				translate(
				    [ outer_size[0] - sidewall - 9.9, hook_pos_1, hook_z ])
				    rotate([ 0, 0, 270 ]) hook(1, 1, 2, 5, 15, 18);

				translate(
				    [ outer_size[0] - sidewall - 9.9, hook_pos_2, hook_z ])
				    rotate([ 0, 0, 270 ]) hook(1, 1, 2, 5, 15, 18);
			}

			// Add the rings for the pcb holes
			scale([ 1, 1, 3 ]) difference() {
				pcb_holes(8, 5);
				pcb_holes();
			}
			translate([ 0, pcb_size[1] - 5, 0 ]) cube([ pcb_size[0], 5, 3 ]);

			// Support for the front lid
			// Sides
			translate([ -10, outer_size[1] - 8, 0 ]) {
				hull() {
					cube([ 2, 2, 16 ]);
					translate([ 0.3, -0.3, 8 ]) rotate([ 0, 0, 45 ])
					    cube([ 3, 2, 16 ], center = true);
				}

				translate([ outer_size[0] - 8, 0, 0 ]) {
					hull() {
						cube([ 2, 2, 16 ]);
						translate([ 1.7, -0.3, 8 ]) rotate([ 0, 0, -45 ])
						    cube([ 3, 2, 16 ], center = true);
					}
				}

				// Bottom
				translate([ outer_size[0] - 20, -2, 0 ]) {
					hull() {
						cube([ 15, 2, 2 ]);
						translate([ 0, 2, 2 ]) cube([ 15, 2, 2 ]);
					}
				}
				translate([ 0, -2, 0 ]) {
					hull() {
						cube([ 27, 2, 2 ]);
						translate([ 0, 2, 2 ]) cube([ 27, 2, 2 ]);
					}
				}
			}

			// Add the calbe holder
			translate([ -10, pcb_size[1] + frontyard + sidewall, 3 ])
			    rotate([ 180, 0, 0 ])
			        cableholder([ 4, 4, 4, 4, 4 ], outer_size[0] - 5, 20);
		}

		// Add the cable holder mounting holes
		translate([ 20, outer_size[1] - 10, -3 ]) {
			// M3 screw
			cylinder(d = 3.2, h = 10);
			// M3 screwhead
			translate([ 0, 0, 1 ]) cylinder(d = 6.4, h = 3, $fn = 6);
		}

		translate([ outer_size[0] - 34, outer_size[1] - 10, -3 ]) {
			// M3 screw
			cylinder(d = 3.2, h = 10);
			// M3 screwhead
			translate([ 0, 0, 1 ]) cylinder(d = 6.4, h = 3, $fn = 6);
		}

		translate([ 0, 0, -bottomwall - 0.1 ])
		    // Mounting holes
		    for (i = mounting_holes) {
			translate(i) cylinder(d = 4, h = 5);
		}

		// Remove the diagonal front parts
		front();
	}
}

module front() {
	translate([ -9.8, pcb_size[1] + frontyard, 3 ]) difference() {
		// TODO set the front panel height correctly
		union() {
			cube([ outer_size[0] - sidewall * 2 - 0.4, sidewall, 13 ]);
			translate([ -sidewall, 0, 13 ])
			    cube([ outer_size[0], sidewall, 16 ]);

			hull() {
				translate([ -sidewall, 0, 13 ])
				    cube([ outer_size[0], sidewall, 0.001 ]);
				translate([ 0, 0, 13 - sidewall - 0.4 ]) cube(
				    [ outer_size[0] - sidewall * 2 - 0.4, sidewall, 0.001 ]);
			}
		}

		translate([ 20, 0, 0 ]) cube([ 60, 15, 4 ]);
	}
	difference() {
		// Add the calbe holder
		translate([ 10, outer_size[1] - 3, 3 ]) rotate([ 180, 0, 0 ])
		    scale([ 1, 1, -1 ]) cableholder([ 4, 4, 4, 4, 4 ], 70, 0);

		// Add the cable holder mounting holes
		translate([ 20, outer_size[1] - 10, -3 ]) {
			// M3 screw
			cylinder(d = 3.2, h = 50);
		}

		translate([ outer_size[0] - 34, outer_size[1] - 10, -3 ]) {
			// M3 screw
			cylinder(d = 3.2, h = 50);
		}
	}
}

module top_model() {
	translate([ 0, -sidewall, 0 ]) intersection() {
		cube(top_size + [ 0, 5, 0 ]);
		hull() {
			translate([ 0, top_size[1], top_size[2] - sidewall ])
			    cube([ top_size[0], sidewall, sidewall ]);

			translate([ 0, sidewall, 0 ])
			    cube([ top_size[0], top_size[1], sidewall ]);

			translate([
				top_size[0] - top_size[2] / 4,
				top_size[2] / 4 + sidewall,
				top_size[2] * 0.75 + 1
			]) sphere(d = top_size[2] / 2);

			translate([
				top_size[2] / 4,
				top_size[2] / 4 + sidewall,
				top_size[2] * 0.75 + 1
			]) sphere(d = top_size[2] / 2);
		}
	}
}

module top() {
	translate([ -9.5 - sidewall * 2, -6, -bottomwall ]) {
		difference() {
			top_model();

			// cube(top_size + [0, 0, outer_size[2]]);
			scale_factor = (outer_size[0]) / (top_size[0]);
			// echo(scale_factor);
			translate([ -(outer_size[0] - top_size[0]) / 2, 2.4, -0 ])
			    scale([ scale_factor, 1, scale_factor ]) top_model();
			// cube(top_size + [-sidewall*2+0.3,0,outer_size[2]-sidewall-15]);

			translate([ top_size[0] / 2, 15, top_size[2] - 0.3 ])
			    // scale([ 0.39, 0.4, 0.4 ])
			    scale([ 2.2, 2.2, 0.4 ]) rotate([ 0, 0, 180 ]) stustanet_logo(
			        height = 1, width = top_size[1] / 2 - 10, center = 1);

			led_holes();
		}
		led_spacer();
	}

	// Add the rails for sliding
	translate([ 0, 0, outer_size[2] - bottomwall ]) {
		// left
		translate([ pcb_size[0], 0, 0 ]) hull() {
			cube([ 4, top_size[1] - sidewall - 11, 0.01 ]);
			translate([ 4, 0, 0 ])
			    cube([ 0.01, top_size[1] - sidewall - 12, 4 ]);
		}
		// right
		translate([ -13, 0, 0 ]) hull() {
			cube([ 4, top_size[1] - sidewall - 11, 0.01 ]);
			translate([ 0, 0, 0 ])
			    cube([ 0.01, top_size[1] - sidewall - 12, 4 ]);
		}
	}

	// Add some hooks
	translate([ -10 - sidewall + 0.4, hook_pos_1, hook_z ])
	    rotate([ 0, 0, 270 ]) hook(-1, 3, 2, 5, 15, 18);

	translate([ -10 - sidewall + 0.4, hook_pos_2, hook_z ])
	    rotate([ 0, 0, 270 ]) hook(-1, 3, 2, 5, 15, 18);

	translate([ outer_size[0] - sidewall - 9.4, hook_pos_1, hook_z ])
	    rotate([ 0, 0, 270 ]) hook(1, 3, 2, 5, 15, 18);

	translate([ outer_size[0] - sidewall - 9.4, hook_pos_2, hook_z ])
	    rotate([ 0, 0, 270 ]) hook(1, 3, 2, 5, 15, 18);
}

module ledbar() {
	led_holes();
	translate([ 21, 85, top_size[2] - 2 ]) {
		for (i = [0:num_leds - 1]) {
			translate([ 0, -i * (12.7) - 9, -5 ]) difference() {
				cube([ 8, 9, 5 ]);
				translate([ 1, 1.5, 0 ])
#cube([ 6, 6, 4 ]);
			}
		}
	}
}

module mountingplate() {
	translate([ 0, 20, -7 ]) cube([ 80, 20, 3 ]);

	translate([ 8, 25, -13 ]) difference() {
		cube([ 20, 10, 7 ]);
		translate([ 5, -5, 3 ]) cube([ 10, 20, 3 ]);
	}
	translate([ 80 - 20 - 8, 25, -13 ]) difference() {
		cube([ 20, 10, 7 ]);
		translate([ 5, -5, 3 ]) cube([ 10, 20, 3 ]);
	}
}

%top();
!front();
//base ();
//ledbar();
//mountingplate();
