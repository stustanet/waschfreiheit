$fn=100;

difference() {
    cylinder(h=7, d=6.5);
    
    cylinder(h=100, d=4.5);
    translate([-0.5, -5, -5])
    cube([15, 15, 15]);
    
    translate([-6.5/2+0.5, 0, 2.5])
    cube([1, 100, 1], center=true);
}
