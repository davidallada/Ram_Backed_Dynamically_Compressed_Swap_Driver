#include <linux/init.h> 
#include <linux/module.h>
#include <linux/kernel.h>





/*
 * enc_size() - calculate size of output file when it will be encoded.
 */

long enc_size( unsigned char *in, long sizein )
{
	int packet_size = 0;
	long sizeout = 0;
	unsigned char *pin = in;

	while( pin < in + sizein ) {
		if( (pin[0] == pin[1]) && (packet_size < (0x7f-1)) ) {
			packet_size++;
			pin++;
		}
		else {
			if( packet_size > 0 ) {
				sizeout += 2;
				pin++;
			}

			packet_size = 0;

			while( pin[0] != pin[1] && !((pin >= in + sizein)
					|| (-packet_size > (0x7f-1))) ) {
				packet_size--;
				pin++;
			}

			sizeout += (-packet_size) + 1;
			packet_size = 0;
		}
	}

	return sizeout;
}


/*
 * dec_size() - calculate size of output file when it will be decoded.
 */

long dec_size( unsigned char *in, long sizein )
{
	unsigned char *pin = in;
	unsigned char packet_header;
	long sizeout = 0;
	int size;

	while( pin < in + sizein ) {
		packet_header = *(pin++);
		size = packet_header & 0x7f;

		if( packet_header & 0x80 ) {
			sizeout += size;
			pin++;
		}
		else {
			sizeout += size;
			pin += size;
		}
	}

	return sizeout;
}



/*
 * rle() - encode data from *in and store it in *out buffer. return
 * the size of *out array.
 */

long rle( unsigned char *in, unsigned char *out, long sizein )
{

  	long max_compressed_size = enc_size(in, sizein);
	//out = vmalloc(max_compressed_size);

	unsigned char *pin = in;
	unsigned char *pout = out;
	unsigned char *ptmp;
	int packet_size = 0;
	int i;

  //printk(KERN_ALERT "Inside the %s function\n", __FUNCTION__);

  //printk(KERN_INFO "Value of size-in is %ld\n", sizein);

	while( pin < in + sizein )
	{
    //printk(KERN_INFO "Inside rle while\n");
		/* look for rle packet */
		if( (pin[0] == pin[1]) && (packet_size < (0x7f-1)) )
		{
			packet_size++;
			pin++;
		}
		else
		{
			if( packet_size > 0 )
			{
				/* write rle header and packet */
				*(pout++) = (1 << 7) + ((packet_size + 1) & 0x7f);
				*(pout++) = *(pin++);
			}

			packet_size = 0;
			ptmp = pin;

			/* look for next rle packet */
			while( pin[0] != pin[1] )
			{
				/* don't overflow buffer */
				if( (pin >= in + sizein) || (-packet_size > (0x7f-1)) )
					break;

				/* skip byte... */
				packet_size--;
				pin++;
			}

			/* write non-rle header */
			*(pout++) = (-packet_size) & 0x7f;

			/* write non-rle packet */
			for( i = 0; i < (-packet_size); i++, pout++, ptmp++ )
				*pout = *ptmp;

			packet_size = 0;
		}
	}

	return (pout - out);
}


/*
 * unrle() - decode data from *in and store it in *out buffer. return
 * the size of *out array.
 */

long unrle( unsigned char *in, unsigned char *out, long sizein )
{
	long max_uncompressed_size = dec_size(in, sizein);
	//out = vmalloc(max_uncompressed_size);
  
	unsigned char *pin = in;
	unsigned char *pout = out;
	unsigned char packet_header;
	int i, size;
  
  //printk(KERN_ALERT "Inside the %s function\n", __FUNCTION__);

  //printk(KERN_INFO "Value of size-in is %ld\n", sizein);

	while( pin < in + sizein )
	{
    //printk(KERN_INFO "Inside unrle while\n");

		/* read first byte */
		packet_header = *(pin++);
		size = packet_header & 0x7f;

		if( packet_header & 0x80 )
		{
			/* run-length packet */
			for( i = 0; i < size; i++, pout++ )
				*pout = *pin;

			pin++;
		}
		else
		{
			/* non run-length packet */
			for( i = 0; i < size; i++, pin++, pout++ )
				*pout = *pin;
		}
	}

	return (pout - out);
}

EXPORT_SYMBOL(unrle);
EXPORT_SYMBOL(rle);
EXPORT_SYMBOL(enc_size);
EXPORT_SYMBOL(dec_size);

int rle_simple_module_init(void) {
	printk(KERN_ALERT "Inside the %s function\n", __FUNCTION__);
	return 0;
}

void rle_simple_module_exit(void) {
	printk(KERN_ALERT "Inside the %s function\n", __FUNCTION__);
}

MODULE_LICENSE("MIT LICENSE");

module_init(rle_simple_module_init);
module_exit(rle_simple_module_exit);
