#include "precomp.h" // include (only) this in every .cpp file
#include <stack>
#include <thread>

// -----------------------------------------------------------
// Initialize the application
//https://www.reddit.com/r/generative/comments/ez9q65/how_to_transform_a_painting_into_a_particle_field/?utm_source=share&utm_medium=web2x
// -----------------------------------------------------------

#define BlackAndWhite false

static Sprite theImage( new Surface( "assets/galaxy2.jpg" ), 1 );
constexpr int PARTICLES = 100000;
constexpr float fade = 31.f / 32.f;
Surface *mainImage;

struct ParticleWhite
{
	float x;
	float y;
	float dx;
	float dy;

	ParticleWhite( float a, float b, float c, float d ) { x = a, y = b, dx = c, dy = d; }
	ParticleWhite( float a, float b ) { x = a, y = b, dx = 0.f, dy = 0.f; }
	ParticleWhite() { x = 0.f, y = 0.f, dx = 0.f, dy = 0.f; }
};

ParticleWhite particles[PARTICLES];

uint mainseed = 0x61ddfea7;
uint XorShift(uint seed = mainseed)
{
	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;
	mainseed = seed;
	return seed;
}


void Game::Init()
{
	mainImage = new Surface( SCRWIDTH, SCRHEIGHT );
	
	for (int i = 0; i < PARTICLES; i++)
	{
		particles[i] = ParticleWhite( XorShift() % SCRWIDTH, XorShift() % SCRHEIGHT, ( XorShift() % 50 ) / 25.f + 1.f, 0.f );
	}

	theImage.Draw( screen, 0, 0 );

	//luminance
	Pixel *p = mainImage->GetBuffer();
	Pixel *p2 = screen->GetBuffer();
	for (int y = 0; y < SCRHEIGHT; y++)
	{
		for (int x = 0; x < SCRWIDTH; x++)
		{
			uint col = p2[x + y * SCRWIDTH];
			int r = ( col & 0xFF0000 ) >> 16;
			int g = ( col & 0x00FF00 ) >> 8;
			int b = col & 0x0000FF;
			
			int l = 299 * r + 587 * g + 114 * b;
			p[x + y * SCRWIDTH] = ( ( l / 1000 ) << 16 ) + ( ( l / 1000 ) << 8 ) + ( l / 1000 );
		}
	}
	screen->Clear(0);

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
	Pixel *p = screen->GetBuffer();
	Pixel *lum = mainImage->GetBuffer();
	Pixel *img = theImage.GetBuffer();
	for ( int y = 0; y < SCRHEIGHT; y++ )
	{
		for ( int x = 0; x < SCRWIDTH; x++ )
		{
			uint col = p[x + y * SCRWIDTH];
			int r = ( ( col & 0xFF0000 ) >> 16 ) * fade;
			int g = ( ( col & 0x00FF00 ) >> 8 ) * fade;
			int b = (col & 0x0000FF) * fade;


			p[x + y * SCRWIDTH] = ( r << 16 ) + ( g << 8 ) + b;

			//p[x + y * SCRWIDTH] = lum[x + y * SCRWIDTH];
		}
	}

	for (int i = 0; i < PARTICLES; i++)
	{
		particles[i].dx = (lum[(int)( particles[i].x + particles[i].y * SCRWIDTH )] & 255) / 512.f;
		particles[i].x += particles[i].dx + 0.05f;
		particles[i].y += particles[i].dy;

		if (particles[i].x > SCRWIDTH)
		{
			particles[i].x = 0;
			particles[i].y = XorShift() % SCRHEIGHT;
		}
#if BlackAndWhite
		p[(int)( particles[i].x + particles[i].y * SCRWIDTH )] = lum[(int)( particles[i].x + particles[i].y * SCRWIDTH )];
#else
		p[(int)( particles[i].x + particles[i].y * SCRWIDTH )] = img[(int)( particles[i].x + particles[i].y * SCRWIDTH )];
#endif
	}

}