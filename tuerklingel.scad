x = 51;
y = 51;
z = 1;

fen_x = 12; // fenestrations on x axis
fen_y = 12; // fenestrations on y axis
fen_size = 4; // size of fenestrations as a % of total axis size

// calculate fenestration size
fen_size_x = fen_size * x / 100;
fen_size_y = fen_size * y / 100;

// calculate space remaining and then divide by number of windows needed + 1 to get the desired size of the struts
strut_x = (x - fen_x * fen_size_x) / (fen_x + 1);
strut_y = (y - fen_y * fen_size_y) / (fen_y + 1);

//cover();
base();

module base() {
    union() {
        difference() {
            cylinder(r=39.4, h=2, center=false, $fn=180);
            cylinder(r=27.5, h=2, center=false, $fn=180);
        }
        difference() {
            cylinder(r=27.5, h=30, center=false, $fn=180);
            cylinder(r=26.5, h=30, center=false, $fn=180);
        }
    }
}

module cover() {
    union() {
        difference() {
            cylinder(r=41, h=27, center=false, $fn=180);
            translate([0,0,z]) cylinder(r=38.5, h=27-z, center=false, $fn=180);
            cylinder(r=x/2-0.5, h=z, center=false, $fn=180);
            translate([0,0,27-z]) cylinder(r=39.5, h=2, center=true, $fn=180);
        }
        translate([-7.5,30,0]) cube([15,2,24]);
        translate([27.5,-8,0]) cube([2,16,24]);
        translate([-x/2,-y/2,0]) front_grid();
        translate([0,0,z-1]) difference() {
            union() {
                difference() {
                    cylinder(r=28, h=4, center=false);
                    cylinder(r=26.5, h=4, center=false);
                }
                translate([0,0,3]) difference() {
                    cylinder(r=28, h=1, center=false);
                    cylinder(r=25, h=1, center=false);
                    cylinder(r1=25.8, r2=26.5, h=1, center=false);
                }
            }
            translate([-28,-8,0]) cube([56,16,4]);
            translate([-8,-28,0]) cube([16,56,4]);
        }
    }
}

module front_grid() {
    // take away windows from fenestrated surface
    difference() {
        cube(size=[x, y, z]); // fenestrated surface
        difference() {
            cube(size=[x, y, z]); // fenestrated surface
            translate([x/2,y/2,0]) cylinder(r=x/2, h=z, center=false);
        }
        for (i = [0:fen_x - 1]) {
            translate([i * (fen_size_x + strut_x) + strut_x, 0, 0])
            for (i = [0:fen_y - 1]) {
                translate([0, i * (fen_size_y + strut_y) + strut_x, 0])
                //cube([fen_size_x, fen_size_y, z+2]); // the fenestrations have to start a bit lower and be a bit taller, so that we don't get 0 sized objects
                cylinder(r=fen_size_x/2, h=z+2, center=false, $fn=24);
            }
        }

    }
}
