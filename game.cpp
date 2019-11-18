#include "precomp.h" // include (only) this in every .cpp file

// -----------------------------------------------------------
// Initialize the application
// -----------------------------------------------------------
constexpr uint CIRCLES = 64;
struct Circle
{
	float X, Y, R;
	uint C;
};

Circle circArray[CIRCLES];
Circle before;
uint currentFit = 0;
uint afterFit = 0;

void Game::Mutate(int t)
{
	before = circArray[t];
	switch ( (int)Rand( 4 ) )
	{
	case 0:
		//Pure Reroll
		circArray[t].X = Rand( 600 );
		circArray[t].Y = Rand( 600 );
		circArray[t].R = Rand( 30 );
		circArray[t].C = RandomUInt();
		break;
	case 1:
		//Kleine move
		circArray[t].X += Rand( 10 ) - 5;
		circArray[t].Y += Rand( 10 ) - 5;
		break;
	case 2:
		//Recolor
		circArray[t].C += ( ( (int)( Rand( 10 ) - 5 ) << 16 ) + ( (int)( Rand( 10 ) - 5 ) << 8 ) + (int)( Rand( 10 ) - 5 ) );
		break;
	case 3:
		//Resize
		circArray[t].R += ( Rand( 10 ) - 5 );
		break;
	}
}

void DrawCircle(Circle c, Surface *screen) 
{

	Pixel *sc = screen->GetBuffer();
	for ( float x = max( 0, (int)(c.X - c.R) ); x < min(600, (int)(c.X + c.R)); x++ )
		for (float y = max(0, (int)(c.Y - c.R)); y < min(600, (int)(c.Y + c.R)); y++) 
		{
			float posx = c.X - x;
			float posy = c.Y - y;
			float dst = posx * posx + posy * posy;
			if ( dst < c.R * c.R)
				sc[(int)x + (int)y * SCRWIDTH] = c.C;
				
		}
}

void Game::Init()
{
	for ( int i = 0; i < CIRCLES; i++ )
	{
		circArray[i].X = Rand( 600 );
		circArray[i].Y = Rand( 600 );
		circArray[i].R = Rand( 30 );
		circArray[i].C = RandomUInt();
	}

}

// -----------------------------------------------------------
// Close down application
// -----------------------------------------------------------
void Game::Shutdown()
{

}


static Sprite goose( new Surface( "assets/mario.png" ), 1 );
static Sprite rotatingGun( new Surface( "assets/aagun.tga" ), 36 );
static int frame = 0;

// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void Game::Tick( float deltaTime )
{
	screen->Clear( 0 );
	goose.SetFrame( 0 );
	goose.Draw( screen, 600, 0 );

	for ( int i = 0; i < CIRCLES; i++ )
		DrawCircle( circArray[i], screen );

	
	currentFit = DetermineFitness();

	int t = Rand( CIRCLES );
	Mutate( t );

	screen->Clear( 0 );
	goose.SetFrame( 0 );
	goose.Draw( screen, 600, 0 );

	for ( int i = 0; i < CIRCLES; i++ )
		DrawCircle( circArray[i], screen );

	afterFit = DetermineFitness();
	if ( afterFit > currentFit )
		circArray[t] = before;

	uint f = DetermineFitness();
	printf("%u\n", f);
	
}

uint Game::DetermineFitness() {
	Pixel *p = screen->GetBuffer();
	uint sum = 0;

	for (int i = 0; i < 600; i++)
		for (int j = 0; j < 600; j++) 
		{
			uint test = p[i + j * 1200] - p[i + 600 + j * 1200];
			uint r = (test & 0xFF0000) >> 16;
			uint g = (test & 0x00FF00) >> 8;
			uint b = test & 0x0000FF;

			uint fit = ( r * r + g * g * 3 + b * b ) >> 8;
			sum += fit;
		}

	return sum;
}
