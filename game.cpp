#include "precomp.h" // include (only) this in every .cpp file
#include <stack>
#include <thread>

// -----------------------------------------------------------
// Initialize the application
// -----------------------------------------------------------
static Sprite copySprite( new Surface( "assets/parrot.png" ), 1 );
static int frame = 0;

constexpr uint TrWH = 24;
constexpr uint VertWH = TrWH + 1;
constexpr uint VertCount = VertWH * VertWH;
constexpr uint TRIANGLES = TrWH * TrWH * 2;
constexpr uint THREADS = 8; //KIEK UIT, groter dan 8 crasht ie ivm seeds
constexpr uint MUTATES = 2;
constexpr int SURFWIDTH = 600;

struct pt
{
	int x;
	int y;

	pt( int a, int b ) { x = a, y = b; }
	pt() { x = 0, y = 0; }
};

pt operator+( const pt &a, const pt &b ) { return pt( a.x + b.x, a.y + b.y ); }

uint colors[THREADS + 1][TRIANGLES];

pt vertices[THREADS + 1][VertCount];
int triIndexes[TRIANGLES * 3];

Surface *screens[THREADS], *mainImage;
uint fitness[THREADS + 1];
thread threads[THREADS];
uint seeds[8] = {0x1bd53af8, 0x178dacf0, 0x65afbe35, 0x123abcf3, 0x83abddc7, 0xcd8efa3b, 0x12345679, 0x98765432};



void CreateBackup( int index )
{
	if ( index == THREADS ) return;

	for ( int i = 0; i < VertCount; i++ )
	{
		vertices[THREADS][i] = vertices[index][i];
	}

	for ( int i = 0; i < TRIANGLES; i++ )
		colors[THREADS][i] = colors[index][i];

	fitness[THREADS] = fitness[index];
}

uint XorShift( int index )
{
	seeds[index] ^= seeds[index] << 13;
	seeds[index] ^= seeds[index] >> 17;
	seeds[index] ^= seeds[index] << 5;
	return seeds[index];
}

float Rfloat( int index, float range ) { return XorShift( index ) * 2.3283064365387e-10f * range; }

void RestoreBackup()
{
	for ( int i = 0; i < VertCount; i++ )
		for ( int j = 0; j < THREADS; j++ )
			vertices[j][i] = vertices[THREADS][i];

	for ( int i = 0; i < TRIANGLES; i++ )
		for ( int j = 0; j < THREADS; j++ )
			colors[j][i] = colors[THREADS][i];
}

uint DetermineFitness( Surface *surf, Surface *ogpic )
{
	Pixel *p = surf->GetBuffer();
	Pixel *pog = ogpic->GetBuffer();
	uint sum = 0;

	for ( int i = 0; i < SURFWIDTH; i++ )
		for ( int j = 0; j < SURFWIDTH; j++ )
		{
			uint test = p[i + j * SURFWIDTH];
			uint og = pog[i + j * SURFWIDTH];

			int r = ( ( test & 0xFF0000 ) >> 16 ) - ( ( og & 0xFF0000 ) >> 16 );
			int g = ( ( test & 0x00FF00 ) >> 8 ) - ( ( og & 0x00FF00 ) >> 8 );
			int b = ( test & 0x0000FF ) - ( og & 0x0000FF );

			uint fit = abs( r ) + abs( g ) + abs( b );
			sum += fit;
		}

	return sum;
}

void Mutate( int index )
{
	int tri;
	pt displacement, ogpos, newpos;
	switch ( (int)Rfloat( index, 3 ) )
	{
	case 0:
		//Displace
		tri = (int)Rfloat( index, VertCount );
		displacement = pt( Rfloat( index, 6 ) - 3, Rfloat( index, 6 ) - 3 );
		ogpos = vertices[index][tri];
		newpos = pt( clamp( ogpos.x + displacement.x, 0, SURFWIDTH - 1 ), clamp( ogpos.y + displacement.y, 0, SURFWIDTH - 1 ) );
		vertices[index][tri] = newpos;

		break;
	case 1:
		//reroll color
		colors[index][(int)Rfloat( index, TRIANGLES )] = XorShift( index );
		break;
	case 2:
		//slight color adjust
		colors[index][(int)Rfloat( index, TRIANGLES )] += ( ( (int)( Rfloat( index, 10 ) - 5 ) << 16 ) + ( (int)( Rfloat( index, 10 ) - 5 ) << 8 ) + (int)( Rfloat( index, 10 ) - 5 ) );
		break;
	}
}

void DrawTriangle( pt q1, pt q2, pt q3, uint col, Surface *screen, int width )
{
	pt top, mid, bot;
	if ( q1.y < q2.y )
		if ( q1.y < q3.y )
		{
			top = q1;
			if ( q2.y < q3.y )
			{
				mid = q2;
				bot = q3;
			}
			else
			{
				mid = q3;
				bot = q2;
			}
		}
		else
		{
			top = q3;
			mid = q1;
			bot = q2;
		}
	else if ( q2.y < q3.y )
	{
		top = q2;
		if ( q1.y < q3.y )
		{
			mid = q1;
			bot = q3;
		}
		else
		{
			mid = q3;
			bot = q1;
		}
	}
	else
	{
		top = q3;
		mid = q2;
		bot = q1;
	}

	/*float left, right;
	left = min( q1.x, min(q2.x, q3.x) );
	right = max( q1.x, max(q2.x, q3.x) );*/

	Pixel *sc = screen->GetBuffer();

	float dx1 = mid.x - top.x, dx2 = bot.x - top.x, dx3 = bot.x - mid.x;
	float dy1 = mid.y - top.y, dy2 = bot.y - top.y, dy3 = bot.y - mid.y;

	if ( dy1 == 0 )
		dy1 = 1.f;
	if ( dy2 == 0 )
		dy2 = 1.f;
	if ( dy3 == 0 )
		dy3 = 1.f;

	float dxy1 = dx1 / dy1, dxy2 = dx2 / dy2, dxy3 = dx3 / dy3;

	float x1, x2;
	int y = top.y;

	if ( top.y == mid.y )
		x1 = min( top.x, mid.x ), x2 = max( top.x, mid.x );
	else
		x1 = top.x, x2 = top.x;

	for ( ; y <= mid.y; y++ )
	{

		for ( int x = min( x1, x2 ); x < max( x1, x2 ); x++ )
			sc[x + y * width] = col;
		if ( y < mid.y )
			x1 += dxy1, x2 += dxy2;
	}

	for ( ; y <= bot.y; y++ )
	{
		for ( int x = min( x1, x2 ); x < max( x1, x2 ); x++ )
			sc[x + y * width] = col;
		if ( y < bot.y )
			x1 += dxy3, x2 += dxy2;
	}
}

void DrawScene( Surface *screen, int index )
{
	screen->Clear( 0 );

	for ( int i = 0; i < TRIANGLES; i++ )
		DrawTriangle( vertices[index][triIndexes[i * 3]], vertices[index][triIndexes[i * 3 + 1]], vertices[index][triIndexes[i * 3 + 2]], colors[index][i], screen, SURFWIDTH );
}

void PictureMutate( int index )
{
	float imax = Rfloat( index, MUTATES ) + 1;
	for ( float i = 0; i < imax; i++ )
		Mutate( index );

	DrawScene( screens[index], index );

	fitness[index] = DetermineFitness( screens[index], mainImage );
}

void DrawBest( Surface *sc )
{
	sc->Clear( 0 );

	for ( int i = 0; i < TRIANGLES; i++ )
		DrawTriangle( vertices[THREADS][triIndexes[i * 3]], vertices[THREADS][triIndexes[i * 3 + 1]], vertices[THREADS][triIndexes[i * 3 + 2]], colors[THREADS][i], sc, SCRWIDTH );

	copySprite.Draw( sc, SURFWIDTH, 0 );
}

int BestFit()
{
	int best = THREADS;

	for ( int i = 0; i < THREADS; i++ )
	{
		if ( fitness[i] < fitness[best] )
			best = i;
	}

	return best;
}

void Game::Init()
{
	mainImage = new Surface( SURFWIDTH, SURFWIDTH );
	copySprite.SetFrame( 0 );

	for ( int i = 0; i < THREADS; i++ )
	{
		screens[i] = new Surface( SURFWIDTH, SURFWIDTH );
		fitness[i] = 0;
	}

	fitness[THREADS] = 0xffffffff;

	for ( float y = 0; y < VertWH; y++ )
		for ( float x = 0; x < VertWH; x++ )
		{
			pt vert = pt( x / (float)TrWH * (float)( SURFWIDTH - 1 ), y / (float)TrWH * (float)( SURFWIDTH - 1 ) );
			for ( int i = 0; i < THREADS + 1; i++ )
				vertices[i][(int)( x + y * VertWH )] = vert;
		}

	int trindex = 0;
	for ( int y = 0; y < TrWH; y++ )
		for ( int x = 0; x < TrWH; x++ ) //Triangle indexes, two triangles per square: first =  topleft, topright, botleft; second = topright, botleft, botright
		{
			//Triangle one (topleft)
			triIndexes[trindex] = x + y * VertWH;
			triIndexes[trindex + 1] = x + 1 + y * VertWH;
			triIndexes[trindex + 2] = x + ( y + 1 ) * VertWH;

			//Triangle one (botright)
			triIndexes[trindex + 3] = x + 1 + y * VertWH;
			triIndexes[trindex + 4] = x + ( y + 1 ) * VertWH;
			triIndexes[trindex + 5] = x + 1 + ( y + 1 ) * VertWH;
			trindex += 6;
		}

	for ( int i = 0; i < TRIANGLES; i++ )
	{
		uint c = XorShift( 0 );
		for ( int j = 0; j < THREADS + 1; j++ )
			colors[j][i] = c;
	}
}

// -----------------------------------------------------------
// Close down application
// -----------------------------------------------------------
void Game::Shutdown()
{
}
int asdfg = 227;
// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void Game::Tick( float deltaTime )
{
	mainImage->Clear( 0 );
	copySprite.Draw( mainImage, 0, 0 );

	RestoreBackup();

	for ( int i = 0; i < THREADS; i++ )
		threads[i] = thread( PictureMutate, i );

	for ( int i = 0; i < THREADS; i++ )
		threads[i].join();

	CreateBackup( BestFit() );

	DrawBest( screen );

	printf( "%u\n", fitness[THREADS] );

	//DrawTriangle( pt( 400, 2.9f ), pt( 0, 3 ), pt( 150, 150 ), 0x00FF0000, screen, 1200 );
}
