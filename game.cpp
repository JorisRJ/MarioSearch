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
Circle before[CIRCLES];
uint currentFit = 0;
uint afterFit = 0;

static Sprite goose( new Surface( "assets/mario.png" ), 1 );
static Sprite rotatingGun( new Surface( "assets/aagun.tga" ), 36 );
static int frame = 0;

void CreateBackup()
{
	for ( int i = 0; i < CIRCLES; i++ )
		before[i] = circArray[i];
}

void RestoreBackup() 
{
	for ( int i = 0; i < CIRCLES; i++ )
		circArray[i] = before[i];
}

void Game::Mutate()
{
	int t = Rand( CIRCLES );
	switch ( (int)Rand( 5 ) )
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
	case 4: 
		//Swap order
		int q = Rand( CIRCLES );
		Circle temp = circArray[t];
		circArray[t] = circArray[q];
		circArray[q] = temp;
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

void DrawScene(Surface *screen)
{
	screen->Clear( 0 );
	goose.SetFrame( 0 );
	goose.Draw( screen, 600, 0 );

	for ( int i = 0; i < CIRCLES; i++ )
		DrawCircle( circArray[i], screen );
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



// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void Game::Tick( float deltaTime )
{
	
	DrawScene( screen ); //Eerste keer tekenen
	
	currentFit = DetermineFitness();
	CreateBackup();

	Mutate(); //Muteren

	DrawScene( screen ); //Tweede keer tekenen

	afterFit = DetermineFitness();
	if ( afterFit > currentFit ) //Bepalen of het beter was
		RestoreBackup();

	uint f = DetermineFitness();
	printf("%u\n", f);
	
}

uint Game::DetermineFitness() {
	Pixel *p = screen->GetBuffer();
	uint sum = 0;

	for (int i = 0; i < 600; i++)
		for (int j = 0; j < 600; j++) 
		{
			uint test = p[i + j * SCRWIDTH] - p[i + 600 + j * SCRWIDTH];
			uint r = (test & 0xFF0000) >> 16;
			uint g = (test & 0x00FF00) >> 8;
			uint b = test & 0x0000FF;

			uint fit = ( r * r + g * g + b * b ) >> 8;
			sum += fit;
		}

	return sum;
}
