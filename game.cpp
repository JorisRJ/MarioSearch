#include "precomp.h" // include (only) this in every .cpp file
#include <stack>
#include <thread>

// -----------------------------------------------------------
// Initialize the application
// -----------------------------------------------------------
static Sprite copySprite( new Surface( "assets/bridge.png" ), 1 );
static int frame = 0;

constexpr uint TrWH = 64;
constexpr uint VertWH = TrWH + 1;
constexpr uint VertCount = VertWH * VertWH;
constexpr uint TRIANGLES = TrWH * TrWH * 2;
constexpr uint THREADS = 4; //KIEK UIT, groter dan 32 crasht ie ivm seeds
constexpr uint SCREENS = THREADS + 1;
constexpr uint MUTATES = 2;
constexpr int SURFWIDTH = 800;

constexpr bool HEADSTART = true;

struct pt
{
	int x;
	int y;

	pt( int a, int b ) { x = a, y = b; }
	pt() { x = 0, y = 0; }
};

pt operator+( const pt &a, const pt &b ) { return pt( a.x + b.x, a.y + b.y ); }
pt operator/( const pt &a, const pt &b ) { return pt( a.x / b.x, a.y / b.y ); }
pt operator/( const pt &a, const int &b ) { return pt( a.x / b, a.y / b ); }

uint colors[SCREENS][VertCount];

pt vertices[SCREENS][VertCount];
int triIndexes[TRIANGLES * 3];

int currentBest = THREADS;

Surface *screens[SCREENS], *mainImage;
uint fitness[SCREENS];
thread threads[THREADS];
uint seeds[33] = {0x1f24df53, 0x3d00f1e2, 0x20edba0f, 0xcf824a02, 0x22f70086, 0x04f5822f, 0x1f77e710, 0x726487a8, 0x0c9e301d,
				  0x7cb76725, 0xbe384623, 0xa4a8281a, 0xb8196289, 0x661a6a59, 0x0c69a855, 0xeaae4344, 0x513dedf7, 0xbe0e3809,
				  0x9c97cf0c, 0x27aafd26, 0xd67ebf99, 0x351b9578, 0x046f1558, 0xfc42a388, 0x83e611e7, 0x39a71f50, 0x85db87e1,
				  0xe34c5e62, 0x4b29382d, 0x2f4b8c20, 0xfba53b71, 0xf62da6cd, 0xdac429bf};

/*void CreateBackup( int index )
{
	if ( index == THREADS ) return;

	for ( int i = 0; i < VertCount; i++ )
	{
		vertices[THREADS][i] = vertices[index][i];
	}

	for ( int i = 0; i < TRIANGLES; i++ )
		colors[THREADS][i] = colors[index][i];

	fitness[THREADS] = fitness[index];
}*/

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
	for ( int j = 0; j < SCREENS; j++ )
	{
		if ( j == currentBest ) continue;
		for ( int i = 0; i < VertCount; i++ )
			vertices[j][i] = vertices[currentBest][i];
	}

	for ( int j = 0; j < SCREENS; j++ )
	{
		if ( j == currentBest ) continue;
		for ( int i = 0; i < VertCount; i++ )
			colors[j][i] = colors[currentBest][i];
	}
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
	tri = (int)Rfloat( index, VertCount );
	switch ( (int)Rfloat( index, 3 ) )
	{
	case 0:
		//Displace
		displacement = pt( Rfloat( index, 6 ) - 3, Rfloat( index, 6 ) - 3 );
		ogpos = vertices[index][tri];
		newpos = pt( clamp( ogpos.x + displacement.x, 0, SURFWIDTH - 1 ), clamp( ogpos.y + displacement.y, 0, SURFWIDTH - 1 ) );
		vertices[index][tri] = newpos;

		break;
	case 1:
		//reroll color
		colors[index][tri] = XorShift( index );
		break;
	case 2:
		//slight color adjust
		colors[index][tri] += ( ( (int)( Rfloat( index, 10 ) - 5 ) << 16 ) + ( (int)( Rfloat( index, 10 ) - 5 ) << 8 ) + (int)( Rfloat( index, 10 ) - 5 ) );
		break;
	}
}

void DrawTriangle( pt q1, pt q2, pt q3, uint col1, uint col2, uint col3, Surface *screen, int width )
{
	pt top, mid, bot;
	uint topc, midc, botc;
	if ( q1.y < q2.y )
		if ( q1.y < q3.y )
		{
			top = q1;
			topc = col1;
			if ( q2.y < q3.y )
			{
				mid = q2;
				midc = col2;
				bot = q3;
				botc = col3;
			}
			else
			{
				mid = q3;
				midc = col3;
				bot = q2;
				botc = col2;
			}
		}
		else
		{
			top = q3;
			topc = col3;
			mid = q1;
			midc = col1;
			bot = q2;
			botc = col2;
		}
	else if ( q2.y < q3.y )
	{
		top = q2;
		topc = col2;
		if ( q1.y < q3.y )
		{
			mid = q1;
			midc = col1;
			bot = q3;
			botc = col3;
		}
		else
		{
			mid = q3;
			midc = col3;
			bot = q1;
			botc = col1;
		}
	}
	else
	{
		top = q3;
		topc = col3;
		mid = q2;
		midc = col2;
		bot = q1;
		botc = col1;
	}

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
		{
			float dst1 = ( ( top.x - x ) * ( top.x - x ) + ( top.y - y ) * ( top.y - y ) );
			float dst2 = ( ( mid.x - x ) * ( mid.x - x ) + ( mid.y - y ) * ( mid.y - y ) );
			float dst3 = ( ( bot.x - x ) * ( bot.x - x ) + ( bot.y - y ) * ( bot.y - y ) );
			if ( dst1 == 0 )
			{
				sc[x + y * width] = topc;
			}
			else if (dst2 == 0)
			{
				sc[x + y * width] = midc;
			}
			else if (dst3 == 0)
			{
				sc[x + y * width] = botc;
			}
			else
			{
				dst1 = 1.f / dst1;
				dst2 = 1.f / dst2;
				dst3 = 1.f / dst3;

				float total = dst1 + dst2 + dst3;
				int red = ( dst1 * ( ( topc >> 16 ) & 255 ) + dst2 * ( ( midc >> 16 ) & 255 ) + dst3 * ( ( botc >> 16 ) & 255 ) ) / total;
				int green = ( dst1 * ( ( topc >> 8 ) & 255 ) + dst2 * ( ( midc >> 8 ) & 255 ) + dst3 * ( ( botc >> 8 ) & 255 ) ) / total;
				int blue = ( dst1 * ( topc & 255 ) + dst2 * ( midc & 255 ) + dst3 * ( botc & 255 ) ) / total;
				uint col = ( red << 16 ) + ( green << 8 ) + blue;
				sc[x + y * width] = col;
			}
		}
		if ( y < mid.y )
			x1 += dxy1, x2 += dxy2;
	}

	for ( ; y <= bot.y; y++ )
	{
		for ( int x = min( x1, x2 ); x < max( x1, x2 ); x++ )
		{
			float dst1 = ( ( top.x - x ) * ( top.x - x ) + ( top.y - y ) * ( top.y - y ) );
			float dst2 = ( ( mid.x - x ) * ( mid.x - x ) + ( mid.y - y ) * ( mid.y - y ) );
			float dst3 = ( ( bot.x - x ) * ( bot.x - x ) + ( bot.y - y ) * ( bot.y - y ) );
			if ( dst1 == 0 )
			{
				sc[x + y * width] = topc;
			}
			else if ( dst2 == 0 )
			{
				sc[x + y * width] = midc;
			}
			else if ( dst3 == 0 )
			{
				sc[x + y * width] = botc;
			}
			else
			{
				dst1 = 1.f / dst1;
				dst2 = 1.f / dst2;
				dst3 = 1.f / dst3;

				float total = dst1 + dst2 + dst3;
				int red = ( dst1 * ( ( topc >> 16 ) & 255 ) + dst2 * ( ( midc >> 16 ) & 255 ) + dst3 * ( ( botc >> 16 ) & 255 ) ) / total;
				int green = ( dst1 * ( ( topc >> 8 ) & 255 ) + dst2 * ( ( midc >> 8 ) & 255 ) + dst3 * ( ( botc >> 8 ) & 255 ) ) / total;
				int blue = ( dst1 * ( topc & 255 ) + dst2 * ( midc & 255 ) + dst3 * ( botc & 255 ) ) / total;
				uint col = ( red << 16 ) + ( green << 8 ) + blue;
				sc[x + y * width] = col;
			}
		}
		if ( y < bot.y )
			x1 += dxy3, x2 += dxy2;
	}
}

void DrawScene( Surface *screen, int index, int width = SURFWIDTH )
{
	screen->Clear( 0 );

	for ( int i = 0; i < TRIANGLES; i++ )
		DrawTriangle( vertices[index][triIndexes[i * 3]], vertices[index][triIndexes[i * 3 + 1]], vertices[index][triIndexes[i * 3 + 2]], colors[index][triIndexes[i * 3]], colors[index][triIndexes[i * 3 + 1]], colors[index][triIndexes[i * 3 + 2]], screen, width );
}

void PictureMutate( int index )
{
	float imax = Rfloat( index, MUTATES ) + 1;
	for ( float i = 0; i < imax; i++ )
		Mutate( index );

	DrawScene( screens[index], index );

	fitness[index] = DetermineFitness( screens[index], mainImage );
}

void BestFit()
{
	for ( int i = 0; i < SCREENS; i++ )
	{
		if ( fitness[i] < fitness[currentBest] )
			currentBest = i;
	}
}

void Game::Init()
{
	mainImage = new Surface( SURFWIDTH, SURFWIDTH );
	copySprite.SetFrame( 0 );

	//Screens init
	for ( int i = 0; i < SCREENS; i++ )
	{
		screens[i] = new Surface( SURFWIDTH, SURFWIDTH );
		fitness[i] = 0xffffffff;
	}

	//Calculate all the vertices
	for ( float y = 0; y < VertWH; y++ )
		for ( float x = 0; x < VertWH; x++ )
		{
			pt vert = pt( x / (float)TrWH * (float)( SURFWIDTH - 1 ), y / (float)TrWH * (float)( SURFWIDTH - 1 ) );
			for ( int i = 0; i < SCREENS; i++ )
				vertices[i][(int)( x + y * VertWH )] = vert;
		}

	//Attach the triangles to the vertices
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

	//Color init
	if ( HEADSTART )
	{
		copySprite.Draw( mainImage, 0, 0 );
		pt q;
		uint c;
		uint r, g, b;
		Pixel *px = mainImage->GetBuffer();

		//Take three spots in a triangle, take the colors, average the color.
		for ( int i = 0; i < VertCount; i++ )
		{
			q = vertices[0][triIndexes[i * 3]];

			c = ( px[q.x + q.y * SURFWIDTH] );

			for ( int j = 0; j < SCREENS; j++ )
				colors[j][i] = c;
		}
	}
	else
	{
		for ( int i = 0; i < VertCount; i++ )
		{
			uint c = XorShift( 0 );
			for ( int j = 0; j < SCREENS; j++ )
				colors[j][i] = c;
		}
	}
}

// -----------------------------------------------------------
// Close down application
// -----------------------------------------------------------
void Game::Shutdown()
{
}
// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void Game::Tick( float deltaTime )
{
	mainImage->Clear( 0 );
	copySprite.Draw( mainImage, 0, 0 );

	RestoreBackup();

	int k = 0;
	for ( int i = 0; i < THREADS; i++ )
	{
		if ( i == currentBest ) k++; //Skip the best

		threads[i] = thread( PictureMutate, i + k );
	}

	for ( int i = 0; i < THREADS; i++ )
		threads[i].join();

	//CreateBackup( BestFit() );

	BestFit();

	DrawScene( screen, currentBest, SCRWIDTH );
	copySprite.Draw( screen, SURFWIDTH, 0 );

	printf( "%u\n", fitness[currentBest] );

	if ( GetAsyncKeyState( 32 ) )
	{
		/*printf("%i\n", SDL_SaveBMP(screen, "assets/MyFirstSavedImage"));*/
	}

	//DrawTriangle( pt( 400, 2.9f ), pt( 0, 3 ), pt( 150, 150 ), 0x00FF0000, screen, 1200 );
}
