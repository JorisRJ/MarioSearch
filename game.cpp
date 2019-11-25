#include "precomp.h" // include (only) this in every .cpp file
#include <stack>
#include <thread>

// -----------------------------------------------------------
// Initialize the application
// -----------------------------------------------------------
constexpr uint CIRCLES = 128;
constexpr uint THREADS = 8; //KIEK UIT, groter dan 8 crasht ie ivm seeds
constexpr uint MUTATES = 3;
constexpr int SURFWIDTH = 600;

struct Circle
{
	float X, Y, R;
	uint C;
};

Circle circArray[THREADS + 1][CIRCLES]; //backup is de laatste
//Circle before[CIRCLES];
Surface *screens[THREADS], *mainImage;
uint fitness[THREADS + 1];
thread threads[THREADS];
uint seeds[8] = {0x1bd53af8, 0x178dacf0, 0x65afbe35, 0x123abcf3, 0x83abddc7, 0xcd8efa3b, 0x12345679, 0x98765432};
//stack<int> rngNums[THREADS];
stack<int> rngNums;

int asdf = 5;
static Sprite goose( new Surface( "assets/mario.png" ), 1 );
static int frame = 0;

void CreateBackup( int index )
{
	if ( index == THREADS ) return;

	for ( int i = 0; i < CIRCLES; i++ )
	{
		circArray[THREADS][i] = circArray[index][i];
	}
	fitness[THREADS] = fitness[index];
}

uint XorShift( int index )
{
	seeds[index] ^= seeds[index] << 13;
	seeds[index] ^= seeds[index] >> 17;
	seeds[index] ^= seeds[index] << 5;
	return seeds[index];
}

/**uint XorShift(int index) 
{
	uint num = (uint)rngNums[index].top();
	rngNums[index].pop();
	return num;
}*/

/*uint XorShift( int index )
{
	uint num = (uint)rngNums.top();
	rngNums.pop();
	return num;
}*/

float Rfloat( int index, float range ) { return XorShift( index ) * 2.3283064365387e-10f * range; }

void RestoreBackup()
{
	for ( int i = 0; i < CIRCLES; i++ )
		for ( int j = 0; j < THREADS; j++ )
			circArray[j][i] = circArray[THREADS][i];
}

uint DetermineFitness( Surface *surf, Surface *ogpic )
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
	int t = (int)Rfloat( index, CIRCLES );
	int q;
	switch ( (int)Rfloat( index, 8 ) )
	{
	case 0:
		//Pure Reroll
		circArray[index][t].X = Rfloat( index, 600 );
		circArray[index][t].Y = Rfloat( index, 600 );
		circArray[index][t].R = Rfloat( index, 30 ) + 10;
		circArray[index][t].C = XorShift( index );
		break;
	case 1:
		//Kleine move
		circArray[index][t].X += Rfloat( index, 10 ) - 5;
		circArray[index][t].Y += Rfloat( index, 10 ) - 5;
		break;
	case 2:
		//Recolor
		circArray[index][t].C += ( ( (int)( Rfloat( index, 10 ) - 5 ) << 16 ) + ( (int)( Rfloat( index, 10 ) - 5 ) << 8 ) + (int)( Rfloat( index, 10 ) - 5 ) );
		break;
	case 3:
		//Resize
		circArray[index][t].R += ( Rfloat( index, 10 ) - 5 );
		break;
	case 4:
		//Swap order
		q = Rfloat( index, CIRCLES );
		Circle temp = circArray[index][t];
		circArray[index][t] = circArray[index][q];
		circArray[index][q] = temp;
		break;
	case 5:
		//Nieuwe kleur
		circArray[index][t].C = XorShift( index );
		break;
	case 6:
		//Grote move
		circArray[index][t].X = Rfloat( index, 600 );
		circArray[index][t].Y = Rfloat( index, 600 );
		break;
	case 7:
		//Radius reroll
		circArray[index][t].R = Rfloat( index, 30 ) + 10;
		break;
	}
}

void DrawCircle( Circle c, Surface *screen, int width )
{

	Pixel *sc = screen->GetBuffer();
	for ( float x = max( 0, (int)( c.X - c.R ) ); x < min( SURFWIDTH, (int)( c.X + c.R ) ); x++ )
		for ( float y = max( 0, (int)( c.Y - c.R ) ); y < min( SURFWIDTH, (int)( c.Y + c.R ) ); y++ )
		{
			float posx = c.X - x;
			float posy = c.Y - y;
			float dst = posx * posx + posy * posy;
			if ( dst < c.R * c.R )
				sc[(int)x + (int)y * width] = c.C;
		}
}

void DrawScene( Surface *screen, int index )
{
	screen->Clear( 0 );

	for ( int i = 0; i < CIRCLES; i++ )
		DrawCircle( circArray[index][i], screen, SURFWIDTH );
}

void PictureMutate( int index )
{
	for ( int i = 0; i < (int)Rfloat(index, MUTATES); i++ )
		Mutate( index );

	DrawScene( screens[index] , index);

	fitness[index] = DetermineFitness( screens[index], mainImage );
}

void DrawBest( Surface *sc )
{
	sc->Clear( 0 );

	for ( int i = 0; i < CIRCLES; i++ )
		DrawCircle( circArray[THREADS][i], sc, SCRWIDTH );

	goose.Draw( sc, 600, 0 );
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
	mainImage = new Surface( 600, 600 );
	goose.SetFrame( 0 );

	for ( int i = 0; i < THREADS; i++ )
	{
		screens[i] = new Surface( 600, 600 );
		fitness[i] = 0;
		//rngNums[i] = stack<int>();
	}
	rngNums = stack<int>();

	//init stack
	/*for ( int i = 0; i < THREADS; i++ )
	{
		int sz = rngNums.size();
		for ( int j = sz; j < 256; j++ )
			rngNums.push( (int)XSnum( i ) );
	}*/

	//Init fitness
	fitness[THREADS] = 0xffffffff;

	for ( int i = 0; i < CIRCLES; i++ )
	{
		circArray[THREADS][i].X = Rfloat( 1, 600 );
		circArray[THREADS][i].Y = Rfloat( 2, 600 );
		circArray[THREADS][i].R = Rfloat( 3, 30 ) + 10;
		circArray[THREADS][i].C = XorShift( 4 );
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
	goose.Draw( mainImage, 0, 0 );

	RestoreBackup();

	/*int sz = rngNums.size();
	for ( int j = sz; j < 256; j++ )
		rngNums.push( (int)XSnum( 1 ) );*/

	for ( int i = 0; i < THREADS; i++ )
		threads[i] = thread( PictureMutate, i );
	

	for ( int i = 0; i < THREADS; i++ )
		threads[i].join();

	CreateBackup( BestFit() );

	DrawBest( screen );

	printf( "%u\n", fitness[THREADS] );

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
