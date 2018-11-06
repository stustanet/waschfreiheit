$fn = 50;
// outer measurement of the PCB with everythin except the sensor
inner = [ 33, 16.5, 21 ];

// Border that we want to overlap onto the pcb
overlap_top = 1;
// should be smaller than bottom offset, else it looks retarded
pcb_base_height = 3;

// hight of Wall around the PCB
bottom_offset = 4;
// width of the wall including the bottom
outerwall = 2;

// the adjusted outerwall of the top part (only side walls!)
outerwall_top = outerwall + 1;

// details of the front yard
// outer size of the front yard
// Distance of the sensor bottom to the pcb
sensorlength = 7.5;
// radius of the magnet
magnetradius = 5.5;
//width of the magnet
magnetwidth = 5.5;
// offset of the right edge of the pcb
magnetoffset = 10.5;
// Depth of the sensor within the magnet
magnetsensoradjust = 3.1;
// For positioning the sensort in the middle of the magnet
// be careful to keep this value << outerwall
magnetheightadjust = 1;
// radius of the measured wire
wireradius = 2;
// radius of the cablebinder holes
cablebinder_width = 3;
cablebinder_height = 1.5;

datawireradius = 2;

//cablebinder_frontyard_wirecap_conncetor_offset
cablebindermagnetdistance = 1.5;
cablebinderwiredistance = 2.5;

// Sidemount configuration
sidemount_offx = 17;
sidemount_cable_offsetz = 1;
sidemount_sizex = datawireradius*2 + cablebinder_width*2 + outerwall*2;
sidemount_sizey = cablebinder_width + outerwall;
sidemount_sizez = outerwall + bottom_offset + sidemount_cable_offsetz + datawireradius;

// WIREHOLDER STUFF
wireholdersize = [15, 12, 8];
wireholder_offsetz = 4.0;

// Connecting pin between top and bottom
connectingpindepth = 1;
connectingpinradius = outerwall/2-0.1;

// The cablebinder holder at the front 

front_gapx = 10 + outerwall;
front_gapz = 0;
front_holex = 5;
//////////////////////////////////////////////////////
// Calculation shit
//////////////////////////////////////////////////////
magnet_center  = [
    outerwall + inner[0] + sensorlength + magnetradius - magnetsensoradjust,
    outerwall + magnetoffset - 0.75 + magnetwidth/2,
    outerwall + magnetradius - magnetheightadjust
];

frontyard = [ 
    sensorlength + magnetradius*2 - magnetsensoradjust , 
    inner[1] + outerwall * 2,
    bottom_offset
];

///////////////////////////////////////////////////////
// Module fun
///////////////////////////////////////////////////////

module magnet() {
    translate(magnet_center)
    rotate([-90, 0, 0])
    cylinder(h=magnetwidth, r=magnetradius, center=true);
    
}

module wire() {
    translate([outerwall + inner[0] + sensorlength + magnetradius - magnetsensoradjust,
        outerwall + inner[1]/2,
        outerwall + magnetradius - magnetheightadjust
    ])
    rotate([90, 0, 0]) {
        
        cylinder(h=inner[0]+outerwall*2, r = wireradius, center=true);
        cylinder(h=inner[0]+outerwall*2, r = wireradius, center=true);
    }
}

module datawire() {
    //Hole for the datawire
    translate([sidemount_offx, 
        -sidemount_sizey/2 + outerwall + overlap_top/2,
        sidemount_sizez])
    rotate([90, 0, 0])
    cylinder(r=datawireradius, h=sidemount_sizey + overlap_top, center=true);
}

module platine_bottom() {
    translate([outerwall, outerwall, outerwall]) {
        cube(inner);
    }
}

module platine() {
    // TODO add sensor?
    translate([outerwall, outerwall, outerwall]) {
        cube([inner[0], inner[1], pcb_base_height]);
        translate([ overlap_top, overlap_top, pcb_base_height])
        cube([inner[0] - overlap_top*2,
            inner[1] - overlap_top*2,
            inner[2] - pcb_base_height]);
        
        translate ([inner[0] - overlap_top,
            magnetoffset,
            3.0
        ])
        cube([
            sensorlength + overlap_top,
            4.2,
            2
        ]);
    }
}

module cablebinder_frontyard(adjustx, y) {
    translate([
        magnet_center[0] + adjustx,
        y,
        magnet_center[2] + bottom_offset + outerwall/2
    ])
    cube([cablebinder_height, cablebinder_width, 999],
        center=true);
}

module cablebinder_frontyard_top(adjustx, y) {
    translate([
        magnet_center[0] + adjustx,
        y,
        magnet_center[2] + bottom_offset + outerwall/2
    ])
    cube([cablebinder_width, cablebinder_height, 999],
        center=true);
}


module cablebinder_sidemount(adjustx) {
    translate([
        sidemount_offx + adjustx,
        -sidemount_sizey/2 + outerwall,
        0
    ])
    cube([cablebinder_height, cablebinder_width, 999],
        center=true);
}

module connectingpin(delta) {
    translate([outerwall + inner[0] + connectingpinradius + 0.1,
            outerwall + 1,
            outerwall + bottom_offset - connectingpindepth])
    cylinder(h=connectingpindepth+delta, r=connectingpinradius+delta);

    translate([outerwall + inner[0] + connectingpinradius + 0.1,
            outerwall + inner[1] - 0.5,
            outerwall + bottom_offset - connectingpindepth])
    cylinder(h=connectingpindepth+delta, r=connectingpinradius+delta);

}

module stustanet_logo(height) {
    linear_extrude(height = height, convexity=100)
        import (file = "logo.dxf");
    
}


module sidemount() {
    // sidemount
    translate([sidemount_offx,
        -sidemount_sizey/2 + outerwall + overlap_top/2,
        sidemount_sizez/2])
    cube([sidemount_sizex, sidemount_sizey  + overlap_top, sidemount_sizez], center=true);

}

module bottom() {
    difference () {
        union() {
            // Main body
            cube([
                inner[0] + outerwall * 2 + frontyard[0],
                inner[1] + outerwall * 2,
                bottom_offset + outerwall
            ]);
            sidemount();
        }
        // PCB hole
        platine();
        platine_bottom();
        magnet();
        wire();
        
        
        translate([1.5, 13.5, 0.3])
        scale([0.15, 0.15, 0.15])
        rotate([180, 0, 0])
        stustanet_logo(1/0.15);
        
        cablebinder_frontyard( wireradius, outerwall + cablebinder_width/2);
        cablebinder_frontyard(-wireradius, outerwall + cablebinder_width/2);
        
        cablebinder_frontyard_top( wireradius + cablebinderwiredistance,
            magnet_center[1] - magnetwidth/2 - cablebindermagnetdistance);
        cablebinder_frontyard_top(-wireradius - cablebinderwiredistance,
            magnet_center[1] - magnetwidth/2 - cablebindermagnetdistance);

        cablebinder_frontyard_top( wireradius + cablebinderwiredistance, 
            magnet_center[1] + magnetwidth/2 + cablebindermagnetdistance);
        cablebinder_frontyard_top(-wireradius - cablebinderwiredistance, 
            magnet_center[1] + magnetwidth/2 + cablebindermagnetdistance);
        
        datawire();
        cablebinder_sidemount(datawireradius);
        cablebinder_sidemount(-datawireradius);
    }
}

module wireholder() {
    difference() {
        translate([magnet_center[0], magnet_center[1], magnet_center[2] + wireholder_offsetz])
            cube(wireholdersize, center=true);

        magnet();
        wire();
        platine();

                cablebinder_frontyard_top( wireradius + cablebinderwiredistance,
            magnet_center[1] - magnetwidth/2 - cablebindermagnetdistance);
        cablebinder_frontyard_top(-wireradius - cablebinderwiredistance,
            magnet_center[1] - magnetwidth/2 - cablebindermagnetdistance);

        cablebinder_frontyard_top( wireradius + cablebinderwiredistance, 
            magnet_center[1] + magnetwidth/2 + cablebindermagnetdistance);
        cablebinder_frontyard_top(-wireradius - cablebinderwiredistance, 
            magnet_center[1] + magnetwidth/2 + cablebindermagnetdistance);
    }
}

module top() {
    // TODO: Die Oberedeckfläche ist zu dich - der Kondensator stößt dagegen
    difference() {
        union() {
            difference() {
                union() {
                    translate([0, 0, pcb_base_height + outerwall+0.1])
                    cube([inner[0] + outerwall*2,
                        inner[1] + outerwall*2,
                        inner[2] - pcb_base_height + outerwall]);
                }
                sidemount();
                platine();
                datawire();
                bottom();
                
                #translate([2.4, 1.5, pcb_base_height + outerwall+0.1 + inner[2] - pcb_base_height + outerwall-0.3])
                scale([0.1, 0.1, 0.1])
                rotate([0, 0, 30])
                stustanet_logo(1/0.1);

            }
            translate([outerwall +inner[0] - front_gapx,
                0,
                outerwall + inner[2] - pcb_base_height - front_gapz])
            cube([
                front_gapx + outerwall,
                inner[1] + outerwall*2,
                front_gapz + outerwall + pcb_base_height
            ]);
        }
        translate([outerwall * 2 + inner[0] - front_gapx,
            0,
            outerwall * 2  + inner[2] - pcb_base_height - front_gapz])
        cube([
            front_holex,
            inner[1] + outerwall*2,
            front_gapz + outerwall + bottom_offset
        ]);
    }
    
}

//platine();
//magnet();
//wire();
//connectingpin();
bottom();
//wireholder();
//top();
/*
        translate([outerwall/2, 
            outerwall+datawireradius,
            backplate_height + outerwall + bottom_offset - 4])
        rotate([0, 90, 180])
        hull() {
            cylinder(h=outerwall, r=datawireradius, center=true);
            #translate([0, datawireradius, 0]) {
                cube([datawireradius*2, datawireradius, outerwall+1], center=true);
            }
        }
*/