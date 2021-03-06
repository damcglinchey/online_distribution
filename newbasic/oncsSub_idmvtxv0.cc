#include "oncsSub_idmvtxv0.h"
#include <cstring>
#include <bitset>

#include <arpa/inet.h>

using namespace std;

oncsSub_idmvtxv0::oncsSub_idmvtxv0(subevtdata_ptr data)
  :oncsSubevent_w4 (data)
{
  _highest_row_overall = -1;
  _is_decoded = 0;
  _highest_chip = -1;
  memset ( chip_row, 0, 512*32*sizeof(unsigned int));
  for ( int i = 0; i < 32; i++) _highest_region[i] = -1;
}
  
oncsSub_idmvtxv0::~oncsSub_idmvtxv0()
{

}


typedef struct 
{
  unsigned char d0[30];
  unsigned char counter;
  unsigned char ruid;
}  data32;


#define CHIPHEADER     1
#define CHIPEMPTYFRAME 2
#define DATASHORT      3
#define DATALONG0      4
#define DATALONG1      5



int *oncsSub_idmvtxv0::decode ()
{
  if ( _is_decoded) return 0;
  _is_decoded = 1;
  
  int i;

  unsigned int *payload = (unsigned int *) &SubeventHdr->data;  // here begins the payload

  int dlength = getLength()-4 - getPadding();
  unsigned char *the_end = ( unsigned char *) &payload[dlength+1];
  
  unsigned char *pos = (unsigned char *) payload;
  unsigned char *start = pos;

  //  cout << hex << " pos = " << (unsigned long long ) pos << "  the end  " << (unsigned long long) the_end << dec << endl;

  /*  
  int framectr = 0;
  for ( int xc = 0; xc < 32*32; xc++)
    {
      if ( framectr ==0 )
	{
	  cout << ">> ";
	}
      else if ( framectr==30 || framectr==31 )
	{
	  cout << "FX ";
	}
      else if (  (framectr+1)%10 ==0 )
	{
	  cout << "-- ";
	}
      else
	{
	  cout << "   ";
	}
	

      cout << setw(3) << framectr++ << "  " << setw(3) << xc << "  " << hex << setw(2) << setfill('0') << (unsigned int) *pos++ << setfill (' ') << dec << endl;
      if ( framectr >= 32) framectr = 0;
    }
  */
  
  pos = (unsigned char *) payload;

  int first =1;

  unsigned char *c;
  unsigned char b;
  unsigned int address;

  int status = 0;

  int go_on =1;
  
  //  cout << hex << " pos = " << (unsigned long long ) pos << "  the end  " << (unsigned long long) the_end << dec << endl;

  // here we remember the chip and the region we are in, preset to -1 for "nowhere" 
  int the_chip  = -1;
  int the_region = -1;

  while ( pos < the_end && go_on )
    {

      data32 *d32 = ( data32*) pos; 
      if (first)   // we skip the first 10 bytes and start with d1
	{
	  c = &d32->d0[10];
	  first =0;
	}
      else
	{
	  c = d32->d0;
	}

      while (  c < &d32->counter && go_on)
	{
	  b = *c;
	  
	  //	  cout << __FILE__ << " " << __LINE__ << " --- next value " << hex << (unsigned int)  b << dec << " at pos " << ( c-start) << endl;
	  // we skip each "id" byte
	  if ( c >  d32->d0 && (c+1 - d32->d0)%10 == 0)
	    {
	      //  cout << "skipping " << (c - start) << endl;
	      c++;
	      continue;
	    }

	  if ( b == 0xff)  // if we find an idle bye, we reset any in-sequence status. 
	    {
	      status = 0;
	    }
	  
	  if (status) // we mop up what we started in the last round -
	              // these are all cases with more than one byte
	    {
	      switch (status)
		{
		case CHIPHEADER:
		  
		  bunchcounter = b;
		  //  cout << __FILE__ << " " << __LINE__ << " chip header, chip id= " << hex << chip_id << " bunchctr= " << bunchcounter << dec << endl;
		  if ( chip_id ==0 )  // for now ... we'll see what gives with multiple chips
		    {
		      the_chip = chip_id;
		      if ( the_chip > _highest_chip) _highest_chip = the_chip; 
		    }
		  status = 0;
		  break;

		case CHIPEMPTYFRAME:
		  
		  bunchcounter = (b >> 16) & 0xff;
		  // cout << __FILE__ << " " << __LINE__ << " chip empty frame " << hex << chip_id << " " << bunchcounter << dec << endl;
		  status = 0;
		  break;

		case DATASHORT:
		  address += b;	
		  // cout << __FILE__ << " " << __LINE__ << " data short report, enc. id " << hex << encoder_id << " address= " << address << dec << endl;
		  // warning: we have a wrong address here - fix this:
		  //if ( address > 1) address=1; 
		  if ( the_region >= 0 && encoder_id >=0 && the_chip >=0)
		    {
		      // the numbering "snakes" its way through a column (fig. 4.5 in the Alpide manual)
		      //  0 1  > >    row 0 
		      //  3 2  < <    row 1
		      //  4 5  > >    row 2
		      //  7 6  < <   so we need to know if we are in an even or odd row
		      int the_row = (address >> 1);
		      if ( the_row > 511)
			{
			  cout << __FILE__ << " " << __LINE__ << " impossible row: " << the_row
			       << " encoder " <<  encoder_id << " addr " << address << endl;
			}
		      else
			{
			  int is_an_odd_row = the_row & 1;
			  int thebit;
			  if ( is_an_odd_row)
			    {
			      thebit = (encoder_id*2) + (  ( address &1) ^ 1 );
			    }
			  else  
			    {
			      thebit = (encoder_id*2) + ( address&1);
			    }
			  //  cout << __FILE__ << " " << __LINE__ << " the bit " << thebit << endl;
			  chip_row[the_row][the_region] |= ( 1<<thebit);
			  if ( the_row > _highest_row_overall)  _highest_row_overall = the_row;

			}
		    }
		  status = 0;
		  break;

		case DATALONG0:
		  status = DATALONG1;
		  break;

		case DATALONG1:
		  status = 0;
		  break;
		  
		}
	      c++;
	      continue;
	    }
	    

	  if ( b == 0xff)  // Idle byte, skip
	    {
	      //cout << __FILE__ << " " << __LINE__ << " IDLE byte " << hex << (unsigned int)  b << dec << endl;
	    }

	  else if ( ( b >> 4) == 0xa) // we have a chip header
	    {
	      chip_id = ( b & 0xf);
	      status = CHIPHEADER;
	    }

	  else if ( ( b >> 4) == 0xb) // we have a chip trailer
	    {
	      chip_id = ( b & 0xf);
	      the_chip = -1;
	      // cout << __FILE__ << " " << __LINE__ << " chip trailer, chip id= " << hex << chip_id << dec << endl;
	      go_on = 0;
	    }

	  else if ( ( b >> 4) == 0xE) // we have a chip empty frame
	    {
	      chip_id = ( b & 0xf);
	      status = CHIPEMPTYFRAME;
	    }

	  else if ( ( b >> 5) == 0x6) // we have a region header
	    {
	      region_id = (b & 0x1f);
	      if ( region_id <32)
		{
		  the_region = region_id;
		  if ( the_chip >=0) _highest_region[the_chip] = region_id;
		}
	      else
		{
		  cout << __FILE__ << " " << __LINE__ << " wrong region header, id=  " << hex << region_id << dec << endl;
		}
	    }

	  else if ( ( b >> 6) == 0x1) // we have a DATA short report
	    {
	      encoder_id = ( b>>2) & 0xf;
	      address = (b & 0x3) << 8;
	      status = DATASHORT;
	    }

	  else if ( ( b >> 6) == 0x0) // we have a DATA long report
	    {
	      // cout << __FILE__ << " " << __LINE__ << " data long report at pos " << ( c - start)  << endl;
	      status = DATALONG0;
	    }

	  else if ( b == 0xF1) // we have a BUSY on
	    {
	      //cout << __FILE__ << " " << __LINE__ << " Busy on "  << endl;
	    }
      
	  else if ( b == 0xF0) // we have a BUSY off
	    {
	      //cout << __FILE__ << " " << __LINE__ << " Busy off "  << endl;
	    }
       
	  else
	    {
	      cout << __FILE__ << " " << __LINE__ << " unexpected word " << hex << (unsigned int) b << dec << " at pos " << ( c-start) << endl;
	    }
	  c++;
	}
      pos += sizeof(*d32);
    }      
  
  
  return 0;
}


int oncsSub_idmvtxv0::iValue(const int ich,const char *what)
{
  decode();
  if ( strcmp(what,"HIGHEST_CHIP") == 0 )
  {
    return _highest_chip;
  }

  else if ( strcmp(what,"HIGHEST_REGION") == 0 )
  {
    if ( ich > _highest_chip) return -1; // no such chip
    return _highest_region[ich];
  }

  else if ( strcmp(what,"HIGHEST_ROW") == 0 )
  {
    return _highest_row_overall;
  }

  return 0;

}

/*
int oncsSub_idmvtxv0::iValue(const int ich,const int hybrid, const char *what)
{

  if ( nhybrids == 0 ) decoded_data1 = decode(&data1_length);

  std::vector<hybriddata*>::iterator it;
  std::vector<report *>::iterator reportit;

  if ( ich < 0) return 0;

  if ( strcmp(what,"RAWSAMPLES") == 0 )
  {
	    
    for ( it = hybridlist.begin(); it != hybridlist.end(); ++it)
      {
	if ( (*it)->hdmi == hybrid )
	  {
	    if ( ich >= (*it)->words) return 0;
	    return (*it)->adc[ich];
	  }
      }
  }

    return 0;
}
*/

int oncsSub_idmvtxv0::iValue(const int chip, const int region, const int row)
{
  decode();

  if ( chip < 0  || chip > _highest_chip) return 0;
  if ( region < 0 || region > _highest_region[chip] ) return 0;
  if ( row < 0    || row > 511) return 0;
  return chip_row[row][region];
 }


void  oncsSub_idmvtxv0::dump ( OSTREAM& os )
{

  identify(os);


  int x;
  decode();
  os << "Number of chips:      " << setw(4) << iValue(0, "HIGHEST_CHIP") +1<< endl;
  os << "Regions:              ";
  for ( int ichip = 0; ichip < iValue(0, "HIGHEST_CHIP") +1; ichip++)
    {
      os << setw(4) << iValue(ichip, "HIGHEST_REGION");
    }
  os << endl;
  os << "Highest populated row " << iValue(0,"HIGHEST_ROW") << endl;

  // now dump the chip info
  
  for ( int ichip = 0; ichip < iValue(0, "HIGHEST_CHIP")+1; ichip++) // go through the chips
    {
      os << "  *** Chip " << ichip << "  ***" << endl;
      for ( int irow = 0; irow < iValue(0,"HIGHEST_ROW")+1; irow++)
	{
	  os << "  Row  Region" << endl;
	  for ( int iregion = 0; iregion < iValue(ichip, "HIGHEST_REGION")+1; iregion++)
	    {
	      os << setw(4) << irow << "  " << setw(4) << iregion << " | ";
	      unsigned int bits =  iValue(0,iregion, irow);
	      for ( int i = 0; i < 32; i++)
		{
		  if ( (bits >> i) & 1)
		    {
		      os << "X";
		    }
		  else
		    {
		      os << "-";
		    }
		}
	      os << endl;
	    }
	  os << endl;
	}
    }
  
}

