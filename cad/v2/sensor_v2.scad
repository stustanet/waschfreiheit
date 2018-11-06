$fn=100;

epsilon = 0.003;

//TODO: Check params
power_cable_dia = 3.5;
ferrite_outer_dia = 10.4;
ferrite_depth = 5.3;
ferrite_width = 2.2;

pcb_size = [14, 11.5, 5];
pcb_cable_guide_width = 5;
pcb_cable_guide_extra_height = 4;
sensor_left_offset = 1;
sensor_height_offset = 2.3;
sensor_pin_size = [3, 0.8, 0.5];
sensor_head_size = [4.1, 3.2, 1.6];
sensor_head_z_pos = -0.3;

data_cable_dia = 4.1;
data_cable_x = 12;
data_cable_y = 5;
data_cable_zip_dist = -0.3;

base_sx = 21;
base_sy = 17;

base_border = [4, 2, 3.5];
base_edge_width = 1;
base_edge_heigth = 1;
base_cutout_x = 4;
base_cutout_y = 11;

top_thickness = 6;

zip_size = [1.4, 2.9];

// Derived parameters
pcb_to_sensor_center = [0, pcb_size[1], 0] + [sensor_left_offset + sensor_pin_size[0] / 2, sensor_pin_size[1], sensor_height_offset + sensor_head_z_pos] + [0, sensor_head_size[1] / 2, sensor_head_size[2] / 2];

ferrite_center = pcb_to_sensor_center - [ferrite_outer_dia / 2 - ferrite_width / 2, 0, 0];

base_size = [base_sx + base_border[0] * 2, base_sy + base_border[1] * 2, base_border[2] + ferrite_center[2]];

module ferrite()
{
    //difference()
    {
        cylinder(h = ferrite_depth, r = ferrite_outer_dia / 2, center = true);
        cylinder(h = ferrite_depth + epsilon, r = ferrite_outer_dia / 2 - ferrite_width, center = true);
    }
}

module power_cable()
{
    cylinder(h = 100, r = power_cable_dia / 2, center = true);
}

module pcb()
{
    
    cube(pcb_size);
    
    translate([0, 0, -pcb_cable_guide_extra_height / 2])
        cube([pcb_size[0], pcb_cable_guide_width, pcb_size[2] + pcb_cable_guide_extra_height]);
    translate([sensor_left_offset, pcb_size[1], sensor_height_offset])
    {
        cube(sensor_pin_size);
        translate([(sensor_pin_size[0] - sensor_head_size[0]) / 2, sensor_pin_size[1], sensor_head_z_pos])
        {
            cube(sensor_head_size);
        }
    }
}

module data_cable()
{
    cylinder(h = 100, r = data_cable_dia / 2);
}


module base_edge()
{
    translate([-epsilon/2, -epsilon/2, base_size[2] - base_edge_heigth + epsilon])
    {
        // normal edges
        translate([0, base_cutout_y - epsilon, 0])
            cube([base_edge_width + epsilon, base_size[1] - base_cutout_y + epsilon, base_edge_heigth]);
        
        translate([base_size[0] - base_edge_width, 0, 0])
            cube([base_edge_width + epsilon, base_size[1] + epsilon, base_edge_heigth]);
        
        translate([base_cutout_x - epsilon, 0, 0])
            cube([base_size[0] - base_cutout_x + epsilon, base_edge_width + epsilon, base_edge_heigth]);
        
        translate([0, base_size[1] - base_edge_width, 0])
            cube([base_size[0] + epsilon, base_edge_width + epsilon, base_edge_heigth]);
        
        // cutout edges
        translate([base_cutout_x - epsilon, 0, 0])
            cube([base_edge_width + epsilon, base_cutout_y, base_edge_heigth]);
        
        translate([base_edge_width, base_cutout_y - epsilon, 0])
            cube([base_cutout_x, base_edge_width + epsilon, base_edge_heigth]);
    }
}


module base()
{
    translate([pcb_size[0] - base_size[0] + base_border[0],
               -base_border[1],
               -base_border[2]])
    {
        difference()
        {
            cube(base_size);
            
            translate([-epsilon, -epsilon, -epsilon/2])
                cube([base_cutout_x, base_cutout_y, base_size[2] + epsilon]);
            
            base_edge();
        }
    }
}


module top()
{
    translate([pcb_size[0] - base_size[0] + base_border[0],
               -base_border[1],
               -base_border[2] + base_size[2]])
    {
        difference()
        {
            cube([base_size[0], base_size[1], top_thickness]);
            
            translate([-epsilon, -epsilon, -epsilon/2])
                cube([base_cutout_x, base_cutout_y, top_thickness + epsilon]);
        }
    }
        
    translate([pcb_size[0] - base_size[0] + base_border[0],
           -base_border[1],
           -base_border[2]])
    {
        base_edge();
    }
}


module zip_tie()
{
    translate([0, 0, -10])
    {
        cube([zip_size[0], zip_size[1], 25]);
    }
}

module bottom_only_zip_tie()
{
    translate([0, 0, ferrite_center[2] - 10 + epsilon])
    {
        cube([zip_size[0], zip_size[1], 10]);
    }
}

module zip_ties()
{
    translate([ferrite_center[0] - power_cable_dia, 3, 0]) zip_tie();
    translate([pcb_size[0] + 1, 3, 0]) zip_tie();
    
    translate([-9, ferrite_center[1] - zip_size[1] / 2, 0]) zip_tie();
    translate([5.5, ferrite_center[1] - zip_size[1] / 2, 0]) zip_tie();
    
    
    // zip ties for the data cable
    
    translate([data_cable_x - data_cable_dia / 2 - zip_size[0], ferrite_center[1] - zip_size[1] / 2, 0])
    {
        translate([-data_cable_zip_dist, 0, 0])
        bottom_only_zip_tie();
        
        translate([data_cable_dia + data_cable_zip_dist + zip_size[0], 0, 0]) bottom_only_zip_tie();
        
        // cutout in the top part for the data cable zip tie
        translate([0, 0, ferrite_center[2] + epsilon])
        cube([data_cable_dia + zip_size[0] * 2, zip_size[1], data_cable_dia / 2 + zip_size[0] + 1]);
    }
}


module components()
{
    pcb();

    translate(ferrite_center)
    rotate([90, 0, 0])
    {
        ferrite();
        power_cable();
    }
    
    translate([data_cable_x, data_cable_y, ferrite_center[2]])
    rotate([90, 0, 180])
    {
        data_cable();
    }
    
    zip_ties();
}


difference()
{
    base();
    %top();
    #components();
}