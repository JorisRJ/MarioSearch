#include "precomp.h" // include (only) this in every .cpp file
#include <thread>

// -----------------------------------------------------------
// Initialize the application
// -----------------------------------------------------------
constexpr uint CIRCLES = 64;
constexpr uint THREADS = 8;
constexpr int SURFWIDTH = 600;

struct Circle
{
	float X, Y, R;
	uint C;
};

Circle circArray[THREADS + 1][CIRCLES]; //backup is de laatste
//Circle before[CIRCLES];
Surface *screens[THREADS], *mainImage;
uint currentFit = 0;
uint afterFit[THREADS];
thread threads[THREADS];

static Sprite goose( new Surface( "assets/mario.png" ), 1 );
static Sprite rotatingGun( new Surface( "assets/aagun.tga" ), 36 );
static int frame = 0;

void CreateBackup(int index)
{
	for ( int i = 0; i < CIRCLES; i++ )
		circArray[THREADS][i] = circArray[index][i];
}

void RestoreBackup() 
{
	for ( int i = 0; i < CIRCLES; i++ )
		for (int j = 0; j < THREADS; j++)
			circArray[j][i] = circArray[THREADS][i];
}

uint DetermineFitness(Surface *surf, Surface *ogpic)
{
	Pixel *p = surf->GetBuffer();
	Pixel *pog = ogpic->GetBuffer();
	uint sum = 0;

	for ( int i = 0; i < 600; i++ )
		for ( int j = 0; j < 600; j++ )
		{
			/*uint test = p[i + j * SCRWIDTH] - p[i + 600 + j * SCRWIDTH];
			uint r = (test & 0xFF0000) >> 16;
			uint g = (test & 0x00FF00) >> 8;
			uint b = test & 0x0000FF;*/

			uint test = p[i + j * SURFWIDTH];
			uint og = pog[i + j * SURFWIDTH];
			int r = ( ( test & 0xFF0000 ) >> 16 ) - ( ( og & 0xFF0000 ) >> 16 );
			int g = ( ( test & 0x00FF00 ) >> 8 ) - ( ( og & 0x00FF00 ) >> 8 );
			int b = ( test & 0x0000FF ) - ( og & 0x0000FF );

			//uint fit = ( r * r+ g * g * 2 + b * b ) >> 8;
			uint fit = abs( r ) + abs( g ) + abs( b );
			sum += fit;
		}

	return sum;
}

void Mutate( int index )
{
	int t = Rand( CIRCLES );
	int q;
	switch ( (int)Rand( 8 ) )
	{
	case 0:
		//Pure Reroll
		circArray[index][t].X = Rand( 600 );
		circArray[index][t].Y = Rand( 600 );
		circArray[index][t].R = Rand( 30 ) + 10;
		circArray[index][t].C = RandomUInt();
		break;
	case 1:
		//Kleine move
		circArray[index][t].X += Rand( 10 ) - 5;
		circArray[index][t].Y += Rand( 10 ) - 5;
		break;
	case 2:
		//Recolor
		circArray[index][t].C += ( ( (int)( Rand( 10 ) - 5 ) << 16 ) + ( (int)( Rand( 10 ) - 5 ) << 8 ) + (int)( Rand( 10 ) - 5 ) );
		break;
	case 3:
		//Resize
		circArray[index][t].R += ( Rand( 10 ) - 5 );
		break;
	case 4: 
		//Swap order
		q = Rand( CIRCLES );
		Circle temp = circArray[index][t];
		circArray[index][t] = circArray[index][q];
		circArray[index][q] = temp;
		break;
	case 5:
		//Nieuwe kleur
		circArray[index][t].C = RandomUInt();
		break;
	case 6:
		//Grote move
		circArray[index][t].X = Rand( 600 );
		circArray[index][t].Y = Rand( 600 );
		break;
	case 7:
		//Radius reroll
		circArray[index][t].R = Rand( 30 ) + 10;
		break;
	
	}
}

void DrawCircle(Circle c, Surface *screen, int width) 
{

	Pixel *sc = screen->GetBuffer();
	for ( float x = max( 0, (int)( c.X - c.R ) ); x < min( SURFWIDTH, (int)( c.X + c.R ) ); x++ )
		for ( float y = max( 0, (int)( c.Y - c.R ) ); y < min( SURFWIDTH, (int)( c.Y + c.R ) ); y++ ) 
		{
			float posx = c.X - x;
			float posy = c.Y - y;
			float dst = posx * posx + posy * posy;
			if ( dst < c.R * c.R)
				sc[(int)x + (int)y * width] = c.C;
				
		}
}

void DrawScene(Surface *screen)
{
	screen->Clear( 0 );

	for ( int i = 0; i < CIRCLES; i++ )
		DrawCircle( circArray[THREADS][i], screen, SURFWIDTH );
}

void PictureMutate(int index) 
{
	for ( int i = 0; i < 3; i++ )
		Mutate( index );

	DrawScene( screens[index] );

	afterFit[index] = DetermineFitness( screens[index], mainImage );

}

void DrawBest(Surface *sc)
{
	sc->Clear( 0 );

	for ( int i = 0; i < CIRCLES; i++ )
		DrawCircle( circArray[THREADS][i], sc, SCRWIDTH );

	goose.Draw( sc, 600, 0 );
}


void Game::Init()
{
	mainImage = new Surface( 600, 600 );

	for ( int i = 0; i < THREADS; i++ ) 
	{
		screens[i] = new Surface( 600, 600 );
		afterFit[i] = 0;
	}


	for ( int i = 0; i < CIRCLES; i++ )
	{
		circArray[THREADS][i].X = Rand( 600 );
		circArray[THREADS][i].Y = Rand( 600 );
		circArray[THREADS][i].R = Rand( 30 ) + 10;
		circArray[THREADS][i].C = RandomUInt();
	}

}

int BestFit() 
{
	int best = THREADS;
	for ( int i = 0; i < THREADS; i++ )
		if ( afterFit[i] < afterFit[best] )
			best = i;

	return best;
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
	goose.Draw( mainImage, 0, 0 );
	
	RestoreBackup();
	
	for (int i = 0; i < THREADS; i++) 
		threads[i] = thread( PictureMutate, i );

	for ( int i = 0; i < THREADS; i++ )
		threads[i].join();

	CreateBackup( BestFit() );

	DrawBest( screen );

	printf( "%u\n", currentFit );
	
	
	/*DrawScene( screen ); //Eerste keer tekenen
	
	currentFit = DetermineFitness();
	CreateBackup();

	for ( int i = 0; i < 3; i++ )
		Mutate(); //Muteren

	DrawScene( screen ); //Tweede keer tekenen

	afterFit = DetermineFitness();
	if ( afterFit > currentFit ) //Bepalen of het beter was
	{
		RestoreBackup();
		printf( "%u\n", currentFit );
	}
	else
		printf( "%u\n", afterFit );	
	*/
}



/*
Individuele cirkel fitness
hierop multithreaden

miss sign * r ipv r * r
*/
