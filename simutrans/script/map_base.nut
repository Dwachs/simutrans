/**
 * Base file for scripted map generation
 */

heightmap <- ""

cities <- []

settings <- {}

settings.water_height <- 0

function create_city(name, x, y, size) {
	cities.append( { name=name, pos={x=x, y=y}, size=size } )
}

// declare getter functions
function get_heightmap_file()
{
	return heightmap;
}

function get_water_height()
{
	return settings.water_height;
}
