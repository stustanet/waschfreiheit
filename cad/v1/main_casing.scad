$fn = 20;

standheight = 3;

// Measurements of the pcb (including mounts)
pcbsize = [57.5, 83, 25 + standheight];

// frontyard size
frontyard_height = 1.6 + standheight;
frontyard = [28, pcbsize[1], pcbsize[2] - frontyard_height];

hole_positions = [[8.25, 3.5],
                  [50.75, 3.5],
                  [50.5, 78],
                  [8.25, 78] 
];

// Wall thickness of the inner part
bottom_backwall = 1;
bottom_outerwall = 2;
bottom_frontwall = 3;

top_heightcorrection = 0.9;
top_outerwall = 3;

wall_hole_positions = [
    [ pcbsize[0] + 8, 25, 0 ],
   [ -7, 10 + bottom_outerwall, 0],
   [ -7, -10 + pcbsize[1] + bottom_outerwall, 0]];

module stustanet_logo(height) {
    linear_extrude(height = height, convexity=100)
        import (file = "logo.dxf");
    
}

module pcb(bottom_height) {
    cube(pcbsize);
    
    // the antenna
    translate([pcbsize[0], 35.5, 6 + standheight])
    rotate([0, 90, 0])
    cylinder(h=27, d=6);
    
    // the usb plug
    translate([pcbsize[0], 44, 14 + standheight]) {
        cube([28, 12, 6]);
        // the cable
        
        translate([23, 1, 2])
        cube([15, 5, 3 + bottom_height * 1000]);
    }
}

// The origin of the sensor is the inside of the mount, -x is the direction of the connector
module sensorplug() {
    rotate([0,0,90]) {
        // The inner with the flat areas
        difference() {
            translate([0, 9, 0])
            rotate([90, 0, 0])
            cylinder(r=8, h=16.5);
            
            translate([0, 0, -8])
            cube([10, 20, 1.2], center=true);
            translate([0, 0, 8])
            cube([10, 20, 1.2], center=true);
        }
        // The Ring
        translate([])
        rotate([90, 0, 0])
        cylinder(d=19, h=2);
        
        // the plug connectors
        translate([0, 19, 0])
        rotate([90, 0, 0])
        cylinder(d=10, h=10);
    }
}

// here also -y is the direction where the plug comes in
// Beware - the audio plug needs min. 2mm of the frontal mount = the wall at the audio plug must be max. 2mm!
module audioplug() {
    rotate([0, 0, 90]) {
        // Base
        rotate([-90, 0, 0])
        cylinder(h=20, d=8.1);

        // Screwing part
        translate([])
        rotate([90, 0, 0])
        cylinder(h=4.5, d=6);
    }
}

module make_pcb_pin(pos) {
    translate(pos)
    translate([0, bottom_outerwall, bottom_backwall])
    cylinder(r=5, h=standheight);
}

module make_pcb_pin_hole(pos) {
    translate(pos)
    translate([0, bottom_outerwall, 0]) {
        // M3 screw
        cylinder(d=3, h=10);
        // M3 screwhead
        cylinder(d=6.4, h = 2, $fn=6);
    }
}

module wall_mount_hole(pos) {
    translate(pos) {
        cylinder(h=bottom_backwall + 100, d = 4);
    }
}

module wall_mount(pos) {
    height = 3;
    difference() {
        translate(pos) {
            hull() {
                cylinder(h=height, d = 10);
                translate([15, 0, height/2])
                cube([20, 18, height], center=true);
            }
        }
        wall_mount_hole(pos);
    }
}


module bottom() {
    difference() {
        union () {
            translate([-bottom_outerwall, 0, 0])
            cube([pcbsize[0] + frontyard[0] + bottom_frontwall + bottom_outerwall, 
                pcbsize[1] + bottom_outerwall * 2,
                pcbsize[2] + bottom_backwall
            ]);
            wall_mount(wall_hole_positions[1]);
            wall_mount(wall_hole_positions[2]);
        }
        
        // Frontyard
        translate([pcbsize[0],
            bottom_outerwall,
            bottom_backwall + frontyard_height])
        cube(frontyard);
        
        translate([0, bottom_outerwall, bottom_backwall])
        pcb(1);

        for (i = hole_positions) {
            make_pcb_pin_hole(i);
        }
        
        wall_mount_hole(wall_hole_positions[0]);

        translate([pcbsize[0] + frontyard[0] + bottom_frontwall, 0, 0]) {
            translate([0, 14 + bottom_outerwall, 17])
            sensorplug();
            
            translate([0, pcbsize[1] - 14 + bottom_outerwall, 17])
            sensorplug();
            
            translate([-2, pcbsize[1]/2 + 8.5, 13])
            audioplug();
        
            cablebinder_width = 3;
            cablebinder_height = 1.5;
            // Cablebinder holder hole
            translate([-5, pcbsize[1]/2 - 5, 25])
            cube([10, cablebinder_width, cablebinder_height]);
        }

        
        ssnscale = 0.35;
        translate([0, 25, 0.3])
        rotate([180, 0, 30])
        scale([ssnscale, ssnscale, 1])
        stustanet_logo(0.3);
        
    }
    for (i = hole_positions) {
        difference() {
            make_pcb_pin(i);
            make_pcb_pin_hole(i);
        }
    }
}

module top() {
    hook_size = 3;
    
    difference() {
        translate([-top_outerwall - bottom_outerwall, -top_outerwall, -top_outerwall - 0.2])
        cube([pcbsize[0] + frontyard[0] + bottom_frontwall + bottom_outerwall + top_outerwall * 1, 
            pcbsize[1] + bottom_outerwall * 2 + top_outerwall * 2,
            pcbsize[2] + bottom_backwall + top_outerwall * 2 + top_heightcorrection
        ]);

        translate([hook_size, hook_size, -5])
        cube([pcbsize[0] + frontyard[0] + bottom_frontwall + bottom_outerwall + top_outerwall * 1 - hook_size * 2, 
            pcbsize[1] + bottom_outerwall * 2 - hook_size * 2,
            10
        ]);

        ssnscale = 0.15;
        translate([85,
            7,
            pcbsize[2] + bottom_backwall + top_outerwall + top_heightcorrection - 0.5])
        rotate([0, 0, 180])
        scale([ssnscale, ssnscale, 1])
        stustanet_logo(0.5);
        
        translate([-bottom_outerwall, 0, 0])
        cube([pcbsize[0] + frontyard[0] + bottom_frontwall + bottom_outerwall, 
            pcbsize[1] + bottom_outerwall * 2,
            pcbsize[2] + bottom_backwall + + top_heightcorrection
        ]);
        scale([1,1,3])
        translate([0,0,-1.5])
        wall_mount(wall_hole_positions[1]);

        scale([1,1,3])
        translate([0,0,-1.5])
        wall_mount(wall_hole_positions[2]);
    
        cablebinder_width = 3;
        cablebinder_height = 1.5;
        // Cablebinder holder hole
        translate([
            pcbsize[0] + frontyard[0] - 3,
            pcbsize[1]/2 - 5, 
            pcbsize[2]])
        cube([cablebinder_height, cablebinder_width, 10]);

    
    }
    
    // The nose
    translate([0,          bottom_outerwall, bottom_backwall] 
            + [pcbsize[0], 44,               14 + standheight]
            + [23,         1,                2] 
            + [6,          0,                3 + top_heightcorrection])
    cube([bottom_outerwall, 5, 3 + 3]);
}

//translate([0, bottom_outerwall, bottom_backwall])
//#pcb();
//sensorplug();
//audioplug();

top();
//bottom();