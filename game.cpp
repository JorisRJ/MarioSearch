#include "precomp.h" // include (only) this in every .cpp file
#include <stack>
#include <thread>

// -----------------------------------------------------------
// Initialize the application
// -----------------------------------------------------------
static Sprite copySprite( new Surface( "assets/bridge.png" ), 1 ); //assets/scifi.jpg
static int frame = 0;

uint DrawEveryXFrames = 60;
uint Xcounter = 0;

uint TrWidth = 64; //128
uint TrHeight = 64; //96
uint VertWidth = TrWidth + 1;
uint VertHeight = TrHeight + 1;
uint VertCount = VertWidth * VertHeight;
uint TRIANGLES = TrWidth * TrHeight * 2;
constexpr uint THREADS = 4; //KIEK UIT, groter dan 32 crasht ie ivm seeds
constexpr uint SCREENS = THREADS + 1;
uint MUTATES = 1; //met nieuwe methode weet ik niet of dit al hoger dan 1 kan
int SURFWIDTH = 800; //1920
int SURFHEIGHT = 800; //1080

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

//uint colors[SCREENS][TRIANGLES];
//pt vertices[SCREENS][VertCount];
//int triIndexes[TRIANGLES * 3];

uint *colors = (uint *)MALLOC64( SCREENS * TRIANGLES * sizeof( uint ) );
pt *vertices = (pt *)MALLOC64( SCREENS * VertCount * sizeof( pt ) );
int *triIndexes = (int *)MALLOC64( TRIANGLES * 3 * sizeof( int ) );

int currentBest = THREADS;

Surface *screens[SCREENS], *mainImage;
uint currentFitness;
uint fitness[SCREENS];
int fitGain[SCREENS];
thread threads[THREADS];

uint seeds[33] = {0x1f24df53, 0x3d00f1e2, 0x20edba0f, 0xcf824a02, 0x22f70086, 0x04f5822f, 0x1f77e710, 0x726487a8, 0x0c9e301d,
				  0x7cb76725, 0xbe384623, 0xa4a8281a, 0xb8196289, 0x661a6a59, 0x0c69a855, 0xeaae4344, 0x513dedf7, 0xbe0e3809,
				  0x9c97cf0c, 0x27aafd26, 0xd67ebf99, 0x351b9578, 0x046f1558, 0xfc42a388, 0x83e611e7, 0x39a71f50, 0x85db87e1,
				  0xe34c5e62, 0x4b29382d, 0x2f4b8c20, 0xfba53b71, 0xf62da6cd, 0xdac429bf};

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
			vertices[j * VertCount + i] = vertices[currentBest * VertCount + i];
	}

	for ( int j = 0; j < SCREENS; j++ )
	{
		if ( j == currentBest ) continue;
		for ( int i = 0; i < TRIANGLES; i++ )
			colors[j * TRIANGLES + i] = colors[currentBest * TRIANGLES + i];
	}
}

uint DetermineFitness( Surface *surf, Surface *ogpic )
{
	Pixel *p = surf->GetBuffer();
	Pixel *pog = ogpic->GetBuffer();
	uint sum = 0;

	for ( int i = 0; i < SURFWIDTH; i++ )
		for ( int j = 0; j < SURFHEIGHT; j++ )
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

uint AbsColDifference( uint c1, uint c2 )
{
	int r = ( ( c1 & 0xFF0000 ) >> 16 ) - ( ( c2 & 0xFF0000 ) >> 16 );
	int g = ( ( c1 & 0x00FF00 ) >> 8 ) - ( ( c2 & 0x00FF00 ) >> 8 );
	int b = ( c1 & 0x0000FF ) - ( c2 & 0x0000FF );

	uint fit = abs( r ) + abs( g ) + abs( b );
	return fit;
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


uint SingleTriangleFitness( pt q1, pt q2, pt q3, uint col, Surface *screen, int width )
{
	DrawTriangle( q1, q2, q3, col, screen, width );

	int sum = 0;
	int top = min( {q1.y, q2.y, q3.y} );
	int bot = max( {q1.y, q2.y, q3.y} );
	int left = min( {q1.x, q2.x, q3.x} );
	int right = max( {q1.x, q2.x, q3.x} );

	Pixel *og = mainImage->GetBuffer();
	Pixel *test = screen->GetBuffer();

	for ( int y = bot; y < top; y++ )
		for ( int x = left; x < right; x++ )
			sum += AbsColDifference( og[x + y * SURFWIDTH], test[x + y * SURFWIDTH] );

	return sum;
}

uint DisplacementTriangleFitness( int vertex, int index )
{
	uint fitness = 0;
	int vertx = vertex % VertWidth;
	int verty = vertex / VertWidth;

	int displacedTriangles[6] =
		{
			2 * vertx - 1 + 2 * ( verty - 1 ) * TrWidth, //linksboven
			2 * vertx + 2 * ( verty - 1 ) * TrWidth,	 //middenboven
			2 * vertx + 1 + 2 * ( verty - 1 ) * TrWidth, //rechtsboven
			2 * vertx - 2 + 2 * verty * TrWidth,		 //linksonder
			2 * vertx - 1 + 2 * verty * TrWidth,		 //middenonder
			2 * vertx + 2 * verty * TrWidth,			 //rechtsonder
		};

	pt adjVertices[7] = 
		{
			vertices[vertex], //midmid
			vertices[vertex - 1], //linksmid
			vertices[vertex - VertWidth], //midboven
			vertices[vertex - VertWidth + 1], //rechtsboven
			vertices[vertex + 1], //midlinks
			vertices[vertex + VertWidth], //midonder
			vertices[vertex + VertWidth - 1], //linksonder
		};

	int minx = adjVertices[0].x, maxx = adjVertices[0].x, miny = adjVertices[0].y, maxy = adjVertices[0].y;

	for (int i = 1; i < 7; i++)
	{
		if ( adjVertices[i].x < minx )
			minx = adjVertices[i].x;

		if ( adjVertices[i].x > maxx )
			maxx = adjVertices[i].x;

		if ( adjVertices[i].y < miny )
			miny = adjVertices[i].y;

		if ( adjVertices[i].y > maxy )
			maxy = adjVertices[i].y;
	}

	int i;
	for ( int j = 0; j < 6; j++ )
	{
		i = displacedTriangles[j];
		DrawTriangle( vertices[index * VertCount + triIndexes[i * 3]], vertices[index * VertCount + triIndexes[i * 3 + 1]], vertices[index * VertCount + triIndexes[i * 3 + 2]], colors[index * TRIANGLES + i], screens[index], SURFWIDTH );
		//fitness += SingleTriangleFitness( vertices[index * VertCount + triIndexes[i * 3]], vertices[index * VertCount + triIndexes[i * 3 + 1]], vertices[index * VertCount + triIndexes[i * 3 + 2]], colors[index * TRIANGLES + i], mainImage, SURFWIDTH );
	}

	Pixel *og = mainImage->GetBuffer();
	Pixel *test = screens[index]->GetBuffer();

	for ( int y = miny; y < maxy; y++ )
		for ( int x = minx; x < maxx; x++ )
			fitness += AbsColDifference( og[x + y * SURFWIDTH], test[x + y * SURFWIDTH] );

	return fitness;
}

void Mutate( int index )
{
	int tri, oldFit, newFit, triangle;
	pt displacement, ogpos, newpos;
	switch ( ( (int)XorShift( index ) ) % 3 )
	{
	case 0:
		//Displace

		while ( true )
		{
			//tri = (int)Rfloat( index, VertCount );
			tri = ((int) XorShift( index )) % VertCount;
			if ( tri > VertWidth && tri < VertCount - VertWidth )
			{
				int exc = tri % VertWidth;
				if ( exc != 0 && exc != VertWidth - 1 )
					break;
			}
		}

		oldFit = DisplacementTriangleFitness( tri, index );

		//displacement = pt( Rfloat( index, 6 ) - 3, Rfloat( index, 6 ) - 3 );
		displacement = pt( ( (int)XorShift( index ) ) % 6 - 3, ( (int)XorShift( index ) ) % 6 - 3 );
		ogpos = vertices[index * VertCount + tri];
		newpos = pt( clamp( ogpos.x + displacement.x, 0, SURFWIDTH - 1 ), clamp( ogpos.y + displacement.y, 0, SURFHEIGHT - 1 ) );
		vertices[index * VertCount + tri] = newpos;

		newFit = DisplacementTriangleFitness( tri, index );
		fitGain[index] += ( oldFit - newFit );

		break;
	case 1:
		//reroll color
		//triangle = (int)Rfloat( index, TRIANGLES );
		triangle = ( (int)XorShift( index ) ) % TRIANGLES;
		oldFit = SingleTriangleFitness( vertices[index * VertCount + triIndexes[triangle * 3]], vertices[index * VertCount + triIndexes[triangle * 3 + 1]], vertices[index * VertCount + triIndexes[triangle * 3 + 2]], colors[index * TRIANGLES + triangle], mainImage, SURFWIDTH );

		colors[index * TRIANGLES + triangle] = XorShift( index );

		newFit = SingleTriangleFitness( vertices[index * VertCount + triIndexes[triangle * 3]], vertices[index * VertCount + triIndexes[triangle * 3 + 1]], vertices[index * VertCount + triIndexes[triangle * 3 + 2]], colors[index * TRIANGLES + triangle], mainImage, SURFWIDTH );
		fitGain[index] += ( oldFit - newFit );
		break;
	case 2:
		//slight color adjust
		triangle = (int)Rfloat( index, TRIANGLES );
		oldFit = SingleTriangleFitness( vertices[index * VertCount + triIndexes[triangle * 3]], vertices[index * VertCount + triIndexes[triangle * 3 + 1]], vertices[index * VertCount + triIndexes[triangle * 3 + 2]], colors[index * TRIANGLES + triangle], mainImage, SURFWIDTH );

		colors[index * TRIANGLES + (int)Rfloat( index, TRIANGLES )] += ( ( ( ( (int)XorShift( index ) ) % 10 - 5 ) << 16 ) + ( ( ( (int)XorShift( index ) ) % 10 - 5 ) << 8 ) + ( (int)XorShift( index ) ) % 10 - 5 );

		newFit = SingleTriangleFitness( vertices[index * VertCount + triIndexes[triangle * 3]], vertices[index * VertCount + triIndexes[triangle * 3 + 1]], vertices[index * VertCount + triIndexes[triangle * 3 + 2]], colors[index * TRIANGLES + triangle], mainImage, SURFWIDTH );
		fitGain[index] += ( oldFit - newFit );
		break;
	}
}

void DrawScene( Surface *screen, int index, int width = SURFWIDTH )
{
	screen->Clear( 0 );

	for ( int i = 0; i < TRIANGLES; i++ )
		DrawTriangle( vertices[index * VertCount + triIndexes[i * 3]], vertices[index * VertCount + triIndexes[i * 3 + 1]], vertices[index * VertCount + triIndexes[i * 3 + 2]], colors[index * TRIANGLES + i], screen, width );
}

void PictureMutate( int index )
{
	//float imax = Rfloat( index, MUTATES ) + 1;
	float imax = MUTATES;
	for ( float i = 0; i < imax; i++ )
		Mutate( index );

	//DrawScene( screens[index], index );

	//fitness[index] = DetermineFitness( screens[index], mainImage );
}

void BestFit()
{
	/*for ( int i = 0; i < SCREENS; i++ )
	{
		if ( fitness[i] < fitness[currentBest] )
			currentBest = i;
	}*/
	for ( int i = 0; i < SCREENS; i++ )
	{
		if ( fitGain[i] > fitGain[currentBest] )
			currentBest = i;
	}
}

void Game::Init()
{
	mainImage = new Surface( SURFWIDTH, SURFHEIGHT );
	copySprite.SetFrame( 0 );

	//Screens init
	for ( int i = 0; i < SCREENS; i++ )
	{
		screens[i] = new Surface( SURFWIDTH, SURFWIDTH );
		fitness[i] = 0xffffffff;
	}

	//Calculate all the vertices
	for ( float y = 0; y < VertHeight; y++ )
		for ( float x = 0; x < VertWidth; x++ )
		{
			pt vert = pt( x / (float)TrWidth * (float)( SURFWIDTH - 1 ), y / (float)TrHeight * (float)( SURFHEIGHT - 1 ) );
			for ( int i = 0; i < SCREENS; i++ )
				vertices[i * VertCount + (int)( x + y * VertWidth )] = vert;
		}

	//Attach the triangles to the vertices
	int trindex = 0;
	for ( int y = 0; y < TrHeight; y++ )
		for ( int x = 0; x < TrWidth; x++ ) //Triangle indexes, two triangles per square: first =  topleft, topright, botleft; second = topright, botleft, botright
		{
			//Triangle one (topleft)
			triIndexes[trindex] = x + y * VertWidth;
			triIndexes[trindex + 1] = x + 1 + y * VertWidth;
			triIndexes[trindex + 2] = x + ( y + 1 ) * VertWidth;

			//Triangle one (botright)
			triIndexes[trindex + 3] = x + 1 + y * VertWidth;
			triIndexes[trindex + 4] = x + ( y + 1 ) * VertWidth;
			triIndexes[trindex + 5] = x + 1 + ( y + 1 ) * VertWidth;
			trindex += 6;
		}

	//Color init
	if ( HEADSTART )
	{
		copySprite.Draw( mainImage, 0, 0 );
		pt q1, q2, q3, avg, t1, t2, t3;
		uint c1, c2, c3, cf;
		uint r, g, b;
		Pixel *px = mainImage->GetBuffer();

		//Take three spots in a triangle, take the colors, average the color.
		for ( int i = 0; i < TRIANGLES; i++ )
		{
			q1 = vertices[triIndexes[i * 3]];
			q2 = vertices[triIndexes[i * 3 + 1]];
			q3 = vertices[triIndexes[i * 3 + 2]];

			avg = ( q1 + q2 + q3 ) / 3;
			t1 = ( avg + q1 ) / 2;
			t2 = ( avg + q2 ) / 2;
			t3 = ( avg + q3 ) / 2;

			c1 = ( px[t1.x + t1.y * SURFWIDTH] );
			c2 = ( px[t2.x + t2.y * SURFWIDTH] );
			c3 = ( px[t3.x + t3.y * SURFWIDTH] );

			r = ( ( c1 >> 16 ) & 255 ) + ( ( c2 >> 16 ) & 255 ) + ( ( c3 >> 16 ) & 255 );
			g = ( ( c1 >> 8 ) & 255 ) + ( ( c2 >> 8 ) & 255 ) + ( ( c3 >> 8 ) & 255 );
			b = ( c1 & 255 ) + ( c2 & 255 ) + ( c3 & 255 );

			cf = ( r / 3 << 16 ) + ( g / 3 << 8 ) + b / 3;

			for ( int j = 0; j < SCREENS; j++ )
				colors[j * TRIANGLES + i] = cf;
		}
	}
	else
	{
		for ( int i = 0; i < TRIANGLES; i++ )
		{
			uint c = XorShift( 0 );
			for ( int j = 0; j < SCREENS; j++ )
				colors[j * TRIANGLES + i] = c;
		}
	}

	//only needs to be drawn once
	mainImage->Clear( 0 );
	copySprite.Draw( mainImage, 0, 0 );


	DrawScene( screens[0], 0, SURFWIDTH );
	currentFitness = DetermineFitness( screens[0], mainImage );

	//First Draw
	DrawScene( screen, currentBest, SCRWIDTH ); 
	copySprite.Draw( screen, SURFWIDTH, 0 );
}

void DrawToFinalScreen( Surface *screen, int index )
{
	Pixel *px1 = screen->GetBuffer();
	Pixel *px2 = screens[index]->GetBuffer();

	for ( int y = 0; y < SURFHEIGHT; y++ )
		for ( int x = 0; x < SURFWIDTH; x++ )
			px1[y * SCRWIDTH + x] = px2[y * SURFWIDTH + x];
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
	/*mainImage->Clear( 0 );
	copySprite.Draw( mainImage, 0, 0 );*/

	for ( int i = 0; i < SCREENS; i++ )
		fitGain[i] = 0;

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

	//DrawToFinalScreen( screen, currentBest );//werkt niet met nieuwe methode
	Xcounter++;
	if (Xcounter == DrawEveryXFrames)
	{
		DrawScene( screen, currentBest, SCRWIDTH ); //werkt wel met nieuwe methode
		copySprite.Draw( screen, SURFWIDTH, 0 );
		Xcounter = 0;
	}
	/*
	wss: eerst de 6 veranderde triangles naar een buffer schrijven dan pas fitness berekenen
	ipv: direct de nieuwe kleur min de col uit de buffer.

	*/


	currentFitness -= fitGain[currentBest];
	printf( "%u\n", currentFitness );
	

	//DrawTriangle( pt( 400, 2.9f ), pt( 0, 3 ), pt( 150, 150 ), 0x00FF0000, screen, 1200 );
}
