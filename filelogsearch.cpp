#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <regex>

int main() 
{	
	std::ifstream file2("msg.txt");
	if (!file2.is_open())
	{
		std::cerr << "Error opening file 2" << std::endl;
		return 1;
	}
	
	
	
	int left = 0;
	file2.seekg(0, std::ios::end);
	int right = static_cast<int>( file2.tellg() );
	int mid;
	
	std::cout << "End " << right << std::endl;
	
	std::string target = "apple";
	
	std::regex line_re("(\\w+)");
	std::smatch match;
	std::string line;
	
	std::string tmp;
  
        int i = 0;
	while(left <= right)
	{
	++i;
	  mid = (left + right) / 2;
	  file2.seekg(mid);
          
          while (file2.tellg() > 0 && file2.peek() != '\n')
            file2.seekg(file2.tellg() - static_cast<std::streamoff>(1));
  
          if (file2.peek() == '\n')
          {
            file2.seekg(file2.tellg() + static_cast<std::streamoff>(1));
            mid = static_cast<int>( file2.tellg() );
	  }
	  
	  auto start = file2.tellg(); //or mid
	  std::getline(file2, line);
	  if (std::regex_search(line, match, line_re))
	    tmp = match[1].str(); 
	  
	  
	  if (target < tmp)
	    right = mid - 1;
	  else if (target > tmp)
	    left = mid + 1;
	  else
	  {
	    std::cout << "Found " << start << std::endl;
	    file2.seekg(start);
	    std::getline(file2, line);
	    std::cout << line << std::endl;
	    break;
	  }
	}
	
	if (left > right)
	{
	  std::cout << "Not found" << std::endl;
	}
	
	file2.close();
	
	std::cout << "i " << i << std::endl;
	
    return 0;
}