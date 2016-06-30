#include "Wave.h"
#include "DrawInfo.h"


using namespace ci;

Wave::Wave( size_t numChunks, Color selectionColor ):
    mNumChunks( numChunks ),
	mSelection( this, selectionColor ),
	mColor(Color(0.5f, 0.5f, 0.5f)),
    mFilterCoeff( 1.0f )
{
	mChunks.reserve( numChunks );

	for ( size_t i = 0; i < numChunks; i++ ){
		mChunks.emplace_back( i );
	}

    auto lambert = gl::ShaderDef().color();
    gl::GlslProgRef shader = gl::getStockShader( lambert );
    mChunkBatch = gl::Batch::create( geom::Rect( ci::Rectf( 0, 0, Chunk::kWidth, 1 ) ), shader );
}

void Wave::reset( bool onlyChunks )
{
	for (size_t i = 0; i < getSize(); i++){
		mChunks[i].reset();
	}

	if (onlyChunks)
		return;

	mSelection.setToNull();
}


void Wave::setChunk(size_t index, float bottom, float top)
{
	Chunk &c = mChunks[index];
	c.setTop(top);
	c.setBottom(bottom);
}

inline const Chunk & Wave::getChunk(size_t index)
{
	return mChunks[index];
}

void Wave::update( double secondsPerChunk, const DrawInfo& di ) {
    typedef std::map<int, Cursor>::iterator MapItr;


    // update the cursor positions
    double now = ci::app::getElapsedSeconds();
    for (MapItr itr = mCursors.begin(); itr != mCursors.end(); ++itr){
        if (mSelection.isNull()){
            itr->second.pos = Cursor::kNoPosition;
        }

        if ( itr->second.pos == Cursor::kNoPosition )
            continue;


        double elapsed = now - itr->second.lastUpdate;

        itr->second.pos = mSelection.getStart() + int( elapsed / secondsPerChunk );

        /* check we don't go too far off */
        if (itr->second.pos > mSelection.getEnd()){
            itr->second.pos = Cursor::kNoPosition;
        }
    }

    // update chunks for animation 
    for ( auto &chunk : mChunks ){
        chunk.update( di );
    }

#ifdef USE_PARTICLES
    mParticleController.updateParticles();
#endif

}

void Wave::draw( const DrawInfo& di ){


	/* ########### draw the particles ########## */
#ifdef USE_PARTICLES
	mParticleController.draw();
#endif

	/* ########### draw the wave ########## */
	/* scale the wave to fit the window */
	gl::pushModelView(); 

	
	const float wavePixelLen =  ( mNumChunks * ( 2 + Chunk::kWidth ) );
	/* scale the x-axis for the wave to fit the window */
	gl::scale( ((float)di.getWindowWidth() ) / wavePixelLen , 1.0f);
	/* draw the chunks */
	if (mSelection.isNull()){
		/* no selection: all chunks the same color */
		gl::color(mColor); 
		for (size_t i = 0; i < getSize(); i++){
			mChunks[i].draw( di, mChunkBatch );
		}
	}
    else{ 
        // Selection not null 
		gl::color(this->mColor); 

        // update the array with cursor positions 
        mCursorsPos.clear();
        for ( auto cursor : mCursors ){
            mCursorsPos.push_back( cursor.second.pos );
        }

		gl::enableAlphaBlending();

		const float selectionAlpha = 0.5f + mFilterCoeff * 0.5f;


		for (size_t i = 0; i < getSize(); i++){
			/* when in selection use selection color */
			
			if (i == mSelection.getStart()){
				/* draw the selection bar with a transparent selection color */
				gl::color(mSelection.getColor().r, mSelection.getColor().g, mSelection.getColor().b, 0.5f);
                mChunks[i].drawBar( di, mChunkBatch );

				/* set the color to the selection */
				gl::color(mSelection.getColor().r, mSelection.getColor().g, mSelection.getColor().b, selectionAlpha);
			}

            // check if one of the cursors is positioned in this chunk  
			if (std::find(mCursorsPos.begin(), mCursorsPos.end(),i) != mCursorsPos.end() ){
				gl::color(CURSOR_CLR);
				mChunks[i].draw( di, mChunkBatch );
				gl::color(mSelection.getColor().r, mSelection.getColor().g, mSelection.getColor().b, selectionAlpha);
			}
			else{
				/* just draw with current color */
				mChunks[i].draw( di, mChunkBatch );
			}
			
			/* exit selection: go back to wave color */
			if (i == mSelection.getEnd()){
				/* draw the selection bar with a transparent selection color */
				gl::color(mSelection.getColor().r, mSelection.getColor().g, mSelection.getColor().b, 0.5f);
                mChunks[i].drawBar( di, mChunkBatch );
				/* set the colo to the wave */
				gl::color(this->mColor);  
			}
		}
		gl::disableAlphaBlending();
	}
	

	gl::popModelView();

}



//**************** Selection ***************//

Wave::Selection::Selection(Wave * w, Color color) : 
    mWave( w ), 
    mSelectionStart( 0 ), 
    mSelectionEnd( 0 ),
    mColor( color ),
    mParticleSpread( 1 )
{}


void Wave::Selection::setStart(size_t start)  {

	/* deselect the previous */
    mWave->mChunks[mSelectionStart].setAsSelectionStart( false );
	/* select the next */
    mWave->mChunks[start].setAsSelectionStart( true );
	
	mNull = false;

    size_t size = getSize();

	mSelectionStart = start;
    mSelectionEnd = start + size - 1;
    if ( mSelectionEnd > mWave->getSize() - 1 )
        mSelectionEnd = mWave->getSize() - 1;

}

void Wave::Selection::setSize(size_t size)  {

    if ( size <= 0 ){
        mNull = true;
        return;
    }

    size -= 1;

    // check boundaries: size cannot bring the selection end beyond the end of the wave 
    if ( mSelectionStart+size >= mWave->mNumChunks ){
        size = mWave->mNumChunks - mSelectionStart - 1;
    }

	/* deselect the previous */
    mWave->mChunks[mSelectionEnd].setAsSelectionEnd( false );

    mSelectionEnd = mSelectionStart + size;
	/* select the next */
    mWave->mChunks[mSelectionEnd].setAsSelectionEnd( true );

	mNull = false;
}


const cinder::Color Wave::CURSOR_CLR = Color(1.f, 1.f, 1.f);

