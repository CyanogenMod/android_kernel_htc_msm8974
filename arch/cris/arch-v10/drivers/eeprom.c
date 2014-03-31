
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <asm/uaccess.h>
#include "i2c.h"

#define D(x)


#define EEPROM_MAJOR_NR 122  
#define EEPROM_MINOR_NR 0

#define INITIAL_WRITEDELAY_US 4000
#define MAX_WRITEDELAY_US 10000 

#define EEPROM_RETRIES 10

#define EEPROM_2KB (2 * 1024)
 
#define EEPROM_8KB (8 * 1024 - 1 ) 
#define EEPROM_16KB (16 * 1024)

#define i2c_delay(x) udelay(x)


struct eeprom_type
{
  unsigned long size;
  unsigned long sequential_write_pagesize;
  unsigned char select_cmd;
  unsigned long usec_delay_writecycles; 
  unsigned long usec_delay_step; 
  int adapt_state; 
  
  
  struct mutex lock;
  int retry_cnt_addr; 
  int retry_cnt_read;
};

static int  eeprom_open(struct inode * inode, struct file * file);
static loff_t  eeprom_lseek(struct file * file, loff_t offset, int orig);
static ssize_t  eeprom_read(struct file * file, char * buf, size_t count,
                            loff_t *off);
static ssize_t  eeprom_write(struct file * file, const char * buf, size_t count,
                             loff_t *off);
static int eeprom_close(struct inode * inode, struct file * file);

static int  eeprom_address(unsigned long addr);
static int  read_from_eeprom(char * buf, int count);
static int eeprom_write_buf(loff_t addr, const char * buf, int count);
static int eeprom_read_buf(loff_t addr, char * buf, int count);

static void eeprom_disable_write_protect(void);


static const char eeprom_name[] = "eeprom";

static struct eeprom_type eeprom;

const struct file_operations eeprom_fops =
{
  .llseek  = eeprom_lseek,
  .read    = eeprom_read,
  .write   = eeprom_write,
  .open    = eeprom_open,
  .release = eeprom_close
};


int __init eeprom_init(void)
{
  mutex_init(&eeprom.lock);

#ifdef CONFIG_ETRAX_I2C_EEPROM_PROBE
#define EETEXT "Found"
#else
#define EETEXT "Assuming"
#endif
  if (register_chrdev(EEPROM_MAJOR_NR, eeprom_name, &eeprom_fops))
  {
    printk(KERN_INFO "%s: unable to get major %d for eeprom device\n",
           eeprom_name, EEPROM_MAJOR_NR);
    return -1;
  }
  
  printk("EEPROM char device v0.3, (c) 2000 Axis Communications AB\n");


  eeprom.size = 0;
  eeprom.usec_delay_writecycles = INITIAL_WRITEDELAY_US;
  eeprom.usec_delay_step = 128;
  eeprom.adapt_state = 0;
  
#ifdef CONFIG_ETRAX_I2C_EEPROM_PROBE
  i2c_start();
  i2c_outbyte(0x80);
  if(!i2c_getack())
  {
    
    int success = 0;
    unsigned char buf_2k_start[16];
    
    
    
    
#define LOC1 8
#define LOC2 (0x1fb) 

     
    i2c_stop();
    eeprom.size = EEPROM_2KB;
    eeprom.select_cmd = 0xA0;   
    eeprom.sequential_write_pagesize = 16;
    if( eeprom_read_buf( 0, buf_2k_start, 16 ) == 16 )
    {
      D(printk("2k start: '%16.16s'\n", buf_2k_start));
    }
    else
    {
      printk(KERN_INFO "%s: Failed to read in 2k mode!\n", eeprom_name);  
    }
    
    
    eeprom.size = EEPROM_16KB;
    eeprom.select_cmd = 0xA0;   
    eeprom.sequential_write_pagesize = 64;

    {
      unsigned char loc1[4], loc2[4], tmp[4];
      if( eeprom_read_buf(LOC2, loc2, 4) == 4)
      {
        if( eeprom_read_buf(LOC1, loc1, 4) == 4)
        {
          D(printk("0 loc1: (%i) '%4.4s' loc2 (%i) '%4.4s'\n", 
                   LOC1, loc1, LOC2, loc2));
#if 0
          if (memcmp(loc1, loc2, 4) != 0 )
          {
            
            printk(KERN_INFO "%s: 16k detected in step 1\n", eeprom_name);
            eeprom.size = EEPROM_16KB;     
            success = 1;
          }
          else
#endif
          {
            
            
            loc1[0] = ~loc1[0];
            if (eeprom_write_buf(LOC1, loc1, 1) == 1)
            {
              D(printk("1 loc1: (%i) '%4.4s' loc2 (%i) '%4.4s'\n", 
                       LOC1, loc1, LOC2, loc2));
              if( eeprom_read_buf(LOC1, tmp, 4) == 4)
              {
                D(printk("2 loc1: (%i) '%4.4s' tmp '%4.4s'\n", 
                         LOC1, loc1, tmp));
                if (memcmp(loc1, tmp, 4) != 0 )
                {
                  printk(KERN_INFO "%s: read and write differs! Not 16kB\n",
                         eeprom_name);
                  loc1[0] = ~loc1[0];
                  
                  if (eeprom_write_buf(LOC1, loc1, 1) == 1)
                  {
                    success = 1;
                  }
                  else
                  {
                    printk(KERN_INFO "%s: Restore 2k failed during probe,"
                           " EEPROM might be corrupt!\n", eeprom_name);
                    
                  }
                  i2c_stop();
                  
                  eeprom.size = EEPROM_2KB;
                  eeprom.select_cmd = 0xA0;   
                  eeprom.sequential_write_pagesize = 16;
                  if( eeprom_write_buf(0, buf_2k_start, 16) == 16)
                  {
                  }
                  else
                  {
                    printk(KERN_INFO "%s: Failed to write back 2k start!\n",
                           eeprom_name);
                  }
                  
                  eeprom.size = EEPROM_2KB;
                }
              }
                
              if(!success)
              {
                if( eeprom_read_buf(LOC2, loc2, 1) == 1)
                {
                  D(printk("0 loc1: (%i) '%4.4s' loc2 (%i) '%4.4s'\n", 
                           LOC1, loc1, LOC2, loc2));
                  if (memcmp(loc1, loc2, 4) == 0 )
                  {
                    
                    
                    printk(KERN_INFO "%s: 2k detected in step 2\n", eeprom_name);
                    loc1[0] = ~loc1[0];
                    if (eeprom_write_buf(LOC1, loc1, 1) == 1)
                    {
                      success = 1;
                    }
                    else
                    {
                      printk(KERN_INFO "%s: Restore 2k failed during probe,"
                             " EEPROM might be corrupt!\n", eeprom_name);
                      
                    }
                    
                    eeprom.size = EEPROM_2KB;     
                  }
                  else
                  {
                    printk(KERN_INFO "%s: 16k detected in step 2\n",
                           eeprom_name);
                    loc1[0] = ~loc1[0];
                    
                    
                    if (eeprom_write_buf(LOC1, loc1, 1) == 1)
                    {
                      success = 1;
                    }
                    else
                    {
                      printk(KERN_INFO "%s: Restore 16k failed during probe,"
                             " EEPROM might be corrupt!\n", eeprom_name);
                    }
                    
                    eeprom.size = EEPROM_16KB;
                  }
                }
              }
            }
          } 
        } 
        if (!success)
        {
          printk(KERN_INFO "%s: Probing failed!, using 2KB!\n", eeprom_name);
          eeprom.size = EEPROM_2KB;               
        }
      } 
    }
  }
  else
  {
    i2c_outbyte(0x00);
    if(!i2c_getack())
    {
      
      eeprom.size = EEPROM_2KB;
    }
    else
    {
      i2c_start();
      i2c_outbyte(0x81);
      if (!i2c_getack())
      {
        eeprom.size = EEPROM_2KB;
      }
      else
      {
        
        i2c_inbyte();
        eeprom.size = EEPROM_8KB;
      }
    }
  }
  i2c_stop();
#elif defined(CONFIG_ETRAX_I2C_EEPROM_16KB)
  eeprom.size = EEPROM_16KB;
#elif defined(CONFIG_ETRAX_I2C_EEPROM_8KB)
  eeprom.size = EEPROM_8KB;
#elif defined(CONFIG_ETRAX_I2C_EEPROM_2KB)
  eeprom.size = EEPROM_2KB;
#endif

  switch(eeprom.size)
  {
   case (EEPROM_2KB):
     printk("%s: " EETEXT " i2c compatible 2kB eeprom.\n", eeprom_name);
     eeprom.sequential_write_pagesize = 16;
     eeprom.select_cmd = 0xA0;
     break;
   case (EEPROM_8KB):
     printk("%s: " EETEXT " i2c compatible 8kB eeprom.\n", eeprom_name);
     eeprom.sequential_write_pagesize = 16;
     eeprom.select_cmd = 0x80;
     break;
   case (EEPROM_16KB):
     printk("%s: " EETEXT " i2c compatible 16kB eeprom.\n", eeprom_name);
     eeprom.sequential_write_pagesize = 64;
     eeprom.select_cmd = 0xA0;     
     break;
   default:
     eeprom.size = 0;
     printk("%s: Did not find a supported eeprom\n", eeprom_name);
     break;
  }

  

  eeprom_disable_write_protect();

  return 0;
}

static int eeprom_open(struct inode * inode, struct file * file)
{
  if(iminor(inode) != EEPROM_MINOR_NR)
     return -ENXIO;
  if(imajor(inode) != EEPROM_MAJOR_NR)
     return -ENXIO;

  if( eeprom.size > 0 )
  {
    
    return 0;
  }

  
  return -EFAULT;
}


static loff_t eeprom_lseek(struct file * file, loff_t offset, int orig)
{
  
  switch (orig)
  {
   case 0:
     file->f_pos = offset;
     break;
   case 1:
     file->f_pos += offset;
     break;
   case 2:
     file->f_pos = eeprom.size - offset;
     break;
   default:
     return -EINVAL;
  }

  
  if (file->f_pos < 0)
  {
    file->f_pos = 0;    
    return(-EOVERFLOW);
  }
  
  if (file->f_pos >= eeprom.size)
  {
    file->f_pos = eeprom.size - 1;
    return(-EOVERFLOW);
  }

  return ( file->f_pos );
}


static int eeprom_read_buf(loff_t addr, char * buf, int count)
{
  return eeprom_read(NULL, buf, count, &addr);
}




static ssize_t eeprom_read(struct file * file, char * buf, size_t count, loff_t *off)
{
  int read=0;
  unsigned long p = *off;

  unsigned char page;

  if(p >= eeprom.size)  
  {
    return -EFAULT;
  }
  
  if (mutex_lock_interruptible(&eeprom.lock))
    return -EINTR;

  page = (unsigned char) (p >> 8);
  
  if(!eeprom_address(p))
  {
    printk(KERN_INFO "%s: Read failed to address the eeprom: "
           "0x%08X (%i) page: %i\n", eeprom_name, (int)p, (int)p, page);
    i2c_stop();
    
    
    mutex_unlock(&eeprom.lock);
    return -EFAULT;
  }

  if( (p + count) > eeprom.size)
  {
    
    count = eeprom.size - p;
  }

  
  i2c_start();

  
  if(eeprom.size < EEPROM_16KB)
  {
    i2c_outbyte( eeprom.select_cmd | 1 | (page << 1) );
  }

  
  read = read_from_eeprom( buf, count);
  
  if(read > 0)
  {
    *off += read;
  }

  mutex_unlock(&eeprom.lock);
  return read;
}


static int eeprom_write_buf(loff_t addr, const char * buf, int count)
{
  return eeprom_write(NULL, buf, count, &addr);
}



static ssize_t eeprom_write(struct file * file, const char * buf, size_t count,
                            loff_t *off)
{
  int i, written, restart=1;
  unsigned long p;

  if (!access_ok(VERIFY_READ, buf, count))
  {
    return -EFAULT;
  }

  
  if (mutex_lock_interruptible(&eeprom.lock))
    return -EINTR;
  for(i = 0; (i < EEPROM_RETRIES) && (restart > 0); i++)
  {
    restart = 0;
    written = 0;
    p = *off;
   
    
    while( (written < count) && (p < eeprom.size))
    {
      
      if(!eeprom_address(p))
      {
        printk(KERN_INFO "%s: Write failed to address the eeprom: "
               "0x%08X (%i) \n", eeprom_name, (int)p, (int)p);
        i2c_stop();
        
        
        mutex_unlock(&eeprom.lock);
        return -EFAULT;
      }
#ifdef EEPROM_ADAPTIVE_TIMING      
      
      if (eeprom.retry_cnt_addr > 0)
      {
        
        D(printk(">D=%i d=%i\n",
               eeprom.usec_delay_writecycles, eeprom.usec_delay_step));

        if (eeprom.usec_delay_step < 4)
        {
          eeprom.usec_delay_step++;
          eeprom.usec_delay_writecycles += eeprom.usec_delay_step;
        }
        else
        {

          if (eeprom.adapt_state > 0)
          {
            
            eeprom.usec_delay_step *= 2;
            if (eeprom.usec_delay_step > 2)
            {
              eeprom.usec_delay_step--;
            }
            eeprom.usec_delay_writecycles += eeprom.usec_delay_step;
          }
          else if (eeprom.adapt_state < 0)
          {
            
            eeprom.usec_delay_writecycles += eeprom.usec_delay_step;
            if (eeprom.usec_delay_step > 1)
            {
              eeprom.usec_delay_step /= 2;
              eeprom.usec_delay_step--;
            }
          }
        }

        eeprom.adapt_state = 1;
      }
      else
      {
        
        D(printk("<D=%i d=%i\n",
               eeprom.usec_delay_writecycles, eeprom.usec_delay_step));
        
        if (eeprom.adapt_state < 0)
        {
          
          if (eeprom.usec_delay_step > 1)
          {
            eeprom.usec_delay_step *= 2;
            eeprom.usec_delay_step--;
            
            if (eeprom.usec_delay_writecycles > eeprom.usec_delay_step)
            {
              eeprom.usec_delay_writecycles -= eeprom.usec_delay_step;
            }
          }
        }
        else if (eeprom.adapt_state > 0)
        {
          
          if (eeprom.usec_delay_writecycles > eeprom.usec_delay_step)
          {
            eeprom.usec_delay_writecycles -= eeprom.usec_delay_step;
          }
          if (eeprom.usec_delay_step > 1)
          {
            eeprom.usec_delay_step /= 2;
            eeprom.usec_delay_step--;
          }
          
          eeprom.adapt_state = -1;
        }

        if (eeprom.adapt_state > -100)
        {
          eeprom.adapt_state--;
        }
        else
        {
          
          D(printk("#Restart\n"));
          eeprom.usec_delay_step++;
        }
      }
#endif 
      
      do
      {
        i2c_outbyte(buf[written]);        
        if(!i2c_getack())
        {
          restart=1;
          printk(KERN_INFO "%s: write error, retrying. %d\n", eeprom_name, i);
          i2c_stop();
          break;
        }
        written++;
        p++;        
      } while( written < count && ( p % eeprom.sequential_write_pagesize ));

      
      i2c_stop();
      i2c_delay(eeprom.usec_delay_writecycles);
    } 
  } 

  mutex_unlock(&eeprom.lock);
  if (written == 0 && p >= eeprom.size){
    return -ENOSPC;
  }
  *off = p;
  return written;
}


static int eeprom_close(struct inode * inode, struct file * file)
{
  
  return 0;
}


static int eeprom_address(unsigned long addr)
{
  int i;
  unsigned char page, offset;

  page   = (unsigned char) (addr >> 8);
  offset = (unsigned char)  addr;

  for(i = 0; i < EEPROM_RETRIES; i++)
  {
    
    i2c_start();

    if(eeprom.size == EEPROM_16KB)
    {
      i2c_outbyte( eeprom.select_cmd ); 
      i2c_getack();
      i2c_outbyte(page); 
    }
    else
    {
      i2c_outbyte( eeprom.select_cmd | (page << 1) ); 
    }
    if(!i2c_getack())
    {
      
      i2c_stop();
      
      i2c_delay(MAX_WRITEDELAY_US / EEPROM_RETRIES * i);
      
     
    }
    else
    {
      i2c_outbyte(offset);
      
      if(!i2c_getack())
      {
        
        i2c_stop();
      }
      else
        break;
    }
  }    

  
  eeprom.retry_cnt_addr = i;
  D(printk("%i\n", eeprom.retry_cnt_addr));
  if(eeprom.retry_cnt_addr == EEPROM_RETRIES)
  {
    
    return 0;
  }
  return 1;
}


static int read_from_eeprom(char * buf, int count)
{
  int i, read=0;

  for(i = 0; i < EEPROM_RETRIES; i++)
  {    
    if(eeprom.size == EEPROM_16KB)
    {
      i2c_outbyte( eeprom.select_cmd | 1 );
    }

    if(i2c_getack())
    {
      break;
    }
  }
  
  if(i == EEPROM_RETRIES)
  {
    printk(KERN_INFO "%s: failed to read from eeprom\n", eeprom_name);
    i2c_stop();
    
    return -EFAULT;
  }

  while( (read < count))
  {    
    if (put_user(i2c_inbyte(), &buf[read++]))
    {
      i2c_stop();

      return -EFAULT;
    }

    if(read < count)
    {
      i2c_sendack();
    }
  }

  
  i2c_stop();

  return read;
}


#define DBP_SAVE(x)
#define ax_printf printk
static void eeprom_disable_write_protect(void)
{
  
  if (eeprom.size == EEPROM_8KB)
  {
    
    i2c_start();
    i2c_outbyte(0xbe);
    if(!i2c_getack())
    {
      DBP_SAVE(ax_printf("Get ack returns false\n"));
    }
    i2c_outbyte(0xFF);
    if(!i2c_getack())
    {
      DBP_SAVE(ax_printf("Get ack returns false 2\n"));
    }
    i2c_outbyte(0x02);
    if(!i2c_getack())
    {
      DBP_SAVE(ax_printf("Get ack returns false 3\n"));
    }
    i2c_stop();

    i2c_delay(1000);

    
    i2c_start();
    i2c_outbyte(0xbe);
    if(!i2c_getack())
    {
      DBP_SAVE(ax_printf("Get ack returns false 55\n"));
    }
    i2c_outbyte(0xFF);
    if(!i2c_getack())
    {
      DBP_SAVE(ax_printf("Get ack returns false 52\n"));
    }
    i2c_outbyte(0x06);
    if(!i2c_getack())
    {
      DBP_SAVE(ax_printf("Get ack returns false 53\n"));
    }
    i2c_stop();
    
    
    i2c_start();
    i2c_outbyte(0xbe);
    if(!i2c_getack())
    {
      DBP_SAVE(ax_printf("Get ack returns false 56\n"));
    }
    i2c_outbyte(0xFF);
    if(!i2c_getack())
    {
      DBP_SAVE(ax_printf("Get ack returns false 57\n"));
    }
    i2c_outbyte(0x06);
    if(!i2c_getack())
    {
      DBP_SAVE(ax_printf("Get ack returns false 58\n"));
    }
    i2c_stop();
    
    
  }
}

module_init(eeprom_init);
